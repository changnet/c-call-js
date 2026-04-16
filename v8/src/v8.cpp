/**
 * v8.cpp - V8 JavaScript Engine Lua Binding
 *
 * 编译为动态链接库(v8.so / v8.dll)，由 Lua 5.4 加载使用。
 * 提供接口：
 *   v8.init(script_path)   -- 初始化V8环境并加载JS脚本
 *   v8.call(func, args)    -- 调用JS函数，args为JSON字符串，返回JSON字符串
 *   v8.uninit()            -- 销毁V8环境
 *   v8.version()           -- 返回V8版本号
 *
 * 兼容 Windows (LLVM/Clang 17) 和 Linux (g++ 14)
 */

#ifdef _WIN32
#define V8_EXPORT_API extern "C" __declspec(dllexport)
#else
#define V8_EXPORT_API extern "C" __attribute__((visibility("default")))
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>
#include <mutex>

#include "include/v8.h"
#include "include/v8-context.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "include/v8-function.h"
#include "include/v8-json.h"
#include "include/v8-locker.h"
#include "include/v8-exception.h"
#include "include/v8-value.h"
#include "include/libplatform/libplatform.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// ============================================================================
// 全局 V8 平台 (进程级别，只初始化一次)
// ============================================================================
static std::unique_ptr<v8::Platform> g_platform;
static std::once_flag g_init_flag;
static bool g_initialized = false;

static void init_v8_platform() {
    std::call_once(g_init_flag, []() {
        // 初始化V8平台
        g_platform = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform(g_platform.get());
        v8::V8::Initialize();
        g_initialized = true;
    });
}

// ============================================================================
// 每个 Lua State 独立的 V8 上下文
// ============================================================================
struct V8Context {
    v8::Isolate* isolate;
    v8::Global<v8::Context> context;
    std::unique_ptr<v8::ArrayBuffer::Allocator> allocator;

    V8Context() : isolate(nullptr) {}
    ~V8Context() {
        if (isolate) {
            {
                v8::Locker locker(isolate);
                v8::Isolate::Scope isolate_scope(isolate);
                context.Reset();
            }
            isolate->Dispose();
            isolate = nullptr;
        }
    }
};

// Lua userdata 中存储的 metatable 名称
static const char* V8_CONTEXT_META = "v8.context";

// ============================================================================
// 工具函数
// ============================================================================

// 读取文件内容
static std::string read_file(const char* path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// 获取 V8 异常信息
static std::string get_exception_string(v8::Isolate* isolate,
                                         v8::TryCatch* try_catch) {
    v8::HandleScope handle_scope(isolate);
    v8::String::Utf8Value exception(isolate, try_catch->Exception());
    const char* exception_string = *exception ? *exception : "<unknown exception>";

    v8::Local<v8::Message> message = try_catch->Message();
    if (message.IsEmpty()) {
        return std::string(exception_string);
    }

    std::string result;

    // 文件名
    v8::String::Utf8Value filename(isolate,
        message->GetScriptOrigin().ResourceName());
    const char* filename_string = *filename ? *filename : "<unknown>";

    int linenum = message->GetLineNumber(isolate->GetCurrentContext()).FromMaybe(0);
    result += std::string(filename_string) + ":" + std::to_string(linenum) +
              ": " + exception_string;

    // 源码行
    v8::MaybeLocal<v8::String> maybe_source_line =
        message->GetSourceLine(isolate->GetCurrentContext());
    if (!maybe_source_line.IsEmpty()) {
        v8::String::Utf8Value sourceline(isolate,
            maybe_source_line.ToLocalChecked());
        if (*sourceline) {
            result += "\n" + std::string(*sourceline);
        }
    }

    // 堆栈
    v8::Local<v8::Value> stack_trace_val;
    if (try_catch->StackTrace(isolate->GetCurrentContext()).ToLocal(&stack_trace_val) &&
        stack_trace_val->IsString()) {
        v8::String::Utf8Value stack_trace(isolate, stack_trace_val);
        if (*stack_trace) {
            result += "\nStack trace:\n" + std::string(*stack_trace);
        }
    }

    return result;
}

// V8 String -> std::string
static std::string v8_to_string(v8::Isolate* isolate, v8::Local<v8::Value> value) {
    v8::String::Utf8Value utf8(isolate, value);
    return *utf8 ? std::string(*utf8) : "";
}

// std::string -> V8 String
static v8::Local<v8::String> string_to_v8(v8::Isolate* isolate, const char* str) {
    return v8::String::NewFromUtf8(isolate, str).ToLocalChecked();
}

// ============================================================================
// 从 Lua State 获取 V8Context
// ============================================================================
static V8Context* get_v8_context(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, V8_CONTEXT_META);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return nullptr;
    }
    V8Context* ctx = (V8Context*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return ctx;
}

// ============================================================================
// Lua 接口实现
// ============================================================================

/**
 * v8.init(script_path) -> boolean, [error_message]
 *
 * 初始化V8环境并加载执行JS脚本文件。
 * 每个 lua_State 维护独立的 V8 Isolate 和 Context。
 */
static int lua_v8_init(lua_State* L) {
    const char* script_path = luaL_checkstring(L, 1);

    // 检查是否已经初始化
    if (get_v8_context(L) != nullptr) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "V8 already initialized for this Lua state");
        return 2;
    }

    // 确保全局V8平台已初始化
    init_v8_platform();
    if (!g_initialized) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to initialize V8 platform");
        return 2;
    }

    // 读取JS文件
    std::string script_source = read_file(script_path);
    if (script_source.empty()) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, (std::string("Failed to read script file: ") + script_path).c_str());
        return 2;
    }

    // 创建 V8Context
    V8Context* ctx = (V8Context*)lua_newuserdata(L, sizeof(V8Context));
    new (ctx) V8Context();

    // 创建 Isolate
    ctx->allocator.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = ctx->allocator.get();
    ctx->isolate = v8::Isolate::New(create_params);

    if (!ctx->isolate) {
        ctx->~V8Context();
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to create V8 isolate");
        return 2;
    }

    // 编译并执行脚本
    {
        v8::Locker locker(ctx->isolate);
        v8::Isolate::Scope isolate_scope(ctx->isolate);
        v8::HandleScope handle_scope(ctx->isolate);

        // 创建 Context
        v8::Local<v8::Context> context = v8::Context::New(ctx->isolate);
        v8::Context::Scope context_scope(context);

        // 持久化 Context
        ctx->context.Reset(ctx->isolate, context);

        // 编译并执行脚本
        v8::TryCatch try_catch(ctx->isolate);

        v8::Local<v8::String> source = string_to_v8(ctx->isolate, script_source.c_str());
        v8::Local<v8::String> origin_str = string_to_v8(ctx->isolate, script_path);
        v8::ScriptOrigin origin(origin_str);

        v8::MaybeLocal<v8::Script> maybe_script =
            v8::Script::Compile(context, source, &origin);

        if (maybe_script.IsEmpty()) {
            std::string err = "Script compilation failed: " +
                              get_exception_string(ctx->isolate, &try_catch);
            ctx->context.Reset();
            ctx->isolate->Dispose();
            ctx->isolate = nullptr;
            lua_pushboolean(L, 0);
            lua_pushstring(L, err.c_str());
            return 2;
        }

        v8::Local<v8::Script> script = maybe_script.ToLocalChecked();
        v8::MaybeLocal<v8::Value> result = script->Run(context);

        if (result.IsEmpty()) {
            std::string err = "Script execution failed: " +
                              get_exception_string(ctx->isolate, &try_catch);
            ctx->context.Reset();
            ctx->isolate->Dispose();
            ctx->isolate = nullptr;
            lua_pushboolean(L, 0);
            lua_pushstring(L, err.c_str());
            return 2;
        }
    }

    // 将 V8Context 存入 Lua registry
    lua_setfield(L, LUA_REGISTRYINDEX, V8_CONTEXT_META);

    lua_pushboolean(L, 1);
    return 1;
}

/**
 * v8.call(func_name, args_json) -> result_json | nil, error_message
 *
 * 调用JS中的全局函数，args_json是JSON字符串作为参数，
 * 返回JSON字符串结果。
 */
static int lua_v8_call(lua_State* L) {
    const char* func_name = luaL_checkstring(L, 1);
    const char* args_json = luaL_optstring(L, 2, "null");

    V8Context* ctx = get_v8_context(L);
    if (!ctx || !ctx->isolate) {
        lua_pushnil(L);
        lua_pushstring(L, "V8 not initialized. Call v8.init() first");
        return 2;
    }

    v8::Locker locker(ctx->isolate);
    v8::Isolate::Scope isolate_scope(ctx->isolate);
    v8::HandleScope handle_scope(ctx->isolate);
    v8::Local<v8::Context> context = ctx->context.Get(ctx->isolate);
    v8::Context::Scope context_scope(context);

    v8::TryCatch try_catch(ctx->isolate);

    // 获取全局函数
    v8::Local<v8::Value> func_val;
    if (!context->Global()->Get(context, string_to_v8(ctx->isolate, func_name))
            .ToLocal(&func_val) || !func_val->IsFunction()) {
        lua_pushnil(L);
        lua_pushstring(L,
            (std::string("JS function not found: ") + func_name).c_str());
        return 2;
    }

    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(func_val);

    // 解析参数JSON
    v8::Local<v8::Value> args_val;
    v8::Local<v8::String> args_str = string_to_v8(ctx->isolate, args_json);

    if (!v8::JSON::Parse(context, args_str).ToLocal(&args_val)) {
        // 如果JSON解析失败，将原始字符串作为参数传入
        args_val = args_str;
    }

    // 调用函数
    v8::Local<v8::Value> argv[1] = { args_val };
    v8::MaybeLocal<v8::Value> maybe_result =
        func->Call(context, context->Global(), 1, argv);

    if (maybe_result.IsEmpty()) {
        std::string err = "JS function call failed: " +
                          get_exception_string(ctx->isolate, &try_catch);
        lua_pushnil(L);
        lua_pushstring(L, err.c_str());
        return 2;
    }

    v8::Local<v8::Value> result = maybe_result.ToLocalChecked();

    // 将结果转为JSON字符串
    if (result->IsUndefined() || result->IsNull()) {
        lua_pushstring(L, "null");
        return 1;
    }

    // 如果结果是字符串，直接返回
    if (result->IsString()) {
        std::string str = v8_to_string(ctx->isolate, result);
        lua_pushstring(L, str.c_str());
        return 1;
    }

    // 其他类型用JSON.stringify
    v8::MaybeLocal<v8::String> maybe_json = v8::JSON::Stringify(context, result);
    if (maybe_json.IsEmpty()) {
        // fallback: ToString
        v8::Local<v8::String> str_val;
        if (result->ToString(context).ToLocal(&str_val)) {
            std::string str = v8_to_string(ctx->isolate, str_val);
            lua_pushstring(L, str.c_str());
        } else {
            lua_pushstring(L, "null");
        }
        return 1;
    }

    std::string json = v8_to_string(ctx->isolate, maybe_json.ToLocalChecked());
    lua_pushstring(L, json.c_str());
    return 1;
}

/**
 * v8.uninit() -> boolean
 *
 * 销毁当前 Lua State 关联的 V8 环境。
 */
static int lua_v8_uninit(lua_State* L) {
    V8Context* ctx = get_v8_context(L);
    if (ctx) {
        ctx->~V8Context();
        // 从 registry 中移除
        lua_pushnil(L);
        lua_setfield(L, LUA_REGISTRYINDEX, V8_CONTEXT_META);
    }
    lua_pushboolean(L, 1);
    return 1;
}

/**
 * v8.version() -> string
 *
 * 返回V8版本号。
 */
static int lua_v8_version(lua_State* L) {
    lua_pushstring(L, v8::V8::GetVersion());
    return 1;
}

// ============================================================================
// Lua 模块注册
// ============================================================================

static const luaL_Reg v8_funcs[] = {
    {"init",    lua_v8_init},
    {"call",    lua_v8_call},
    {"uninit",  lua_v8_uninit},
    {"version", lua_v8_version},
    {NULL, NULL}
};

extern "C" __attribute__((visibility("default")))
int luaopen_v8(lua_State* L) {
    luaL_newlib(L, v8_funcs);
    return 1;
}

// g++ -shared -fPIC -o v8.so v8.cpp -std=c++20 -O2 -fno-rtti -I../v8 -I../v8/include -DV8_COMPRESS_POINTERS -L../v8/lib -lv8_monolith -llua -fuse-ld=lld -lpthread -ldl -latomic -Wl,-Bsymbolic
