#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#include "quickjs-libc.h"

// 错误处理函数
static void dump_error(JSContext* ctx) {
    JSValue exception = JS_GetException(ctx);
    const char* str = JS_ToCString(ctx, exception);
    if (str) {
        fprintf(stderr, "Error: %s\n", str);
        JS_FreeCString(ctx, str);
    }
    JS_FreeValue(ctx, exception);
}

// 加载JS文件并返回全局对象
static JSValue eval_file(JSContext* ctx, const char* filename) {
    uint8_t* buf;
    size_t buf_len;
    JSValue val, global;
    int ret = 0;

    buf = js_load_file(ctx, &buf_len, filename);
    if (!buf) {
        fprintf(stderr, "Could not load %s\n", filename);
        return JS_EXCEPTION;
    }

    val = JS_Eval(ctx, (char*)buf, buf_len, filename,
                  JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    js_free(ctx, buf);

    if (JS_IsException(val)) {
        dump_error(ctx);
        return JS_EXCEPTION;
    }

    // 如果是模块，需要执行它
    if (JS_VALUE_GET_TAG(val) == JS_TAG_MODULE) {
        JSValue promise = JS_EvalFunction(ctx, val);
        if (JS_IsException(promise)) {
            dump_error(ctx);
            JS_FreeValue(ctx, val);
            return JS_EXCEPTION;
        }
        // 等待模块加载完成
        js_std_loop(ctx);
        JS_FreeValue(ctx, promise);
    }
    
    JS_FreeValue(ctx, val);

    // 返回全局对象
    global = JS_GetGlobalObject(ctx);
    return global;
}

// 调用JS函数
static JSValue call_js_function(JSContext* ctx, JSValue global, 
                              const char* func_name, 
                              int argc, JSValueConst* argv) {
    JSValue func, ret;

    // 获取函数
    func = JS_GetPropertyStr(ctx, global, func_name);
    if (JS_IsException(func)) {
        fprintf(stderr, "Function '%s' not found\n", func_name);
        return JS_EXCEPTION;
    }

    // 调用函数
    ret = JS_Call(ctx, func, global, argc, argv);
    JS_FreeValue(ctx, func);

    if (JS_IsException(ret)) {
        dump_error(ctx);
        return JS_EXCEPTION;
    }

    return ret;
}

int main(int argc, char** argv) {
    JSRuntime* rt;
    JSContext* ctx;
    int ret = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.js>\n", argv[0]);
        return 1;
    }

    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);

    // 初始化
    js_std_init_handlers(rt);
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");
    JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);
    js_std_add_helpers(ctx, -1, NULL);

    // 加载JS文件并获取全局对象
    JSValue global = eval_file(ctx, argv[1]);
    if (!JS_IsException(global)) {
        // 准备参数
        JSValue args[2];
        args[0] = JS_NewInt32(ctx, 42);
        args[1] = JS_NewString(ctx, "test");

        // 调用全局函数
        JSValue result = call_js_function(ctx, global, "testFunction", 2, args);
        
        if (!JS_IsException(result)) {
            // 处理返回值
            if (JS_IsString(result)) {
                const char* str = JS_ToCString(ctx, result);
                printf("Function returned: %s\n", str);
                JS_FreeCString(ctx, str);
            } else if (JS_IsNumber(result)) {
                int32_t num;
                JS_ToInt32(ctx, &num, result);
                printf("Function returned: %d\n", num);
            }
            JS_FreeValue(ctx, result);
        } else {
            ret = 1;
        }

        // 清理参数
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, global);
    } else {
        ret = 1;
    }

    // 清理
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    return ret;
}
