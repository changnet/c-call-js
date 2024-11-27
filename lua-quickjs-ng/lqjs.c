#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "quickjs-ng/quickjs.h"

static JSRuntime *rt = NULL;
static JSContext *ctx = NULL;
char *script_text;
int isInit = 0; //标识是否初始化
JSValue global_obj, script, JSFunc;

void cleanQuickjs()
{
    JS_FreeValue(ctx, JSFunc);
    JS_FreeValue(ctx, global_obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    free(script_text);
}

JSValue execJs(const char *jscmd, const char *args)
{
	JSValue cmd = JS_NewString(ctx, jscmd);
    JSValue cpath = JS_NewString(ctx, args);
    JSValue argv[] = {cmd, cpath};
    JSValue res = JS_Call(ctx, JSFunc, JS_UNDEFINED, 2, argv); //加载js配置
   
    JS_FreeValue(ctx, cmd);
    JS_FreeValue(ctx, cpath);
    return res;
}

int initQuickjs(const char *jsPath, const char *jsFuncName, const char *configPath)
{
	rt = JS_NewRuntime();
	ctx = JS_NewContext(rt);

	// 注册全局对象
    global_obj = JS_GetGlobalObject(ctx);

	// 读取并执行JavaScript脚本文件
    FILE *fp = fopen(jsPath, "r");
    if (!fp) {
        printf("open file fail:%s\n", jsPath);
        return 1; // 文件读取失败
    }

     fseek(fp, 0, SEEK_END);
    long script_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    script_text = malloc(script_size + 1);
    fread(script_text, script_size, 1, fp);
    script_text[script_size] = '\0'; // 添加结束符

    fclose(fp);

    script = JS_Eval(ctx, script_text, script_size, "<memory>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(script)) 
    {
        printf("js eval fail:%s\n", jsPath);
        printf("Error : %s\n", JS_ToCString(ctx, script));
    	cleanQuickjs();
    	return 2; // 文件加载失败
    }

    JSFunc = JS_GetPropertyStr(ctx, global_obj, jsFuncName); // 获取函数
    if (!JS_IsFunction(ctx, JSFunc)) 
    {
        printf("function not found:%s\n", jsFuncName);
    	cleanQuickjs();
    	return 3; // 函数获取失败
    }

    const char * cmd = "INIT";
    JSValue res = execJs(cmd, configPath);
    if (JS_IsException(res))
	{
       	printf("js init fail\n");
	   	printf("Error : %s\n", JS_ToCString(ctx, res));
	   	JS_FreeValue(ctx, res);
	   	cleanQuickjs();
       	return 4; 
    }
    JS_FreeValue(ctx, res);

    isInit = 1;
    return 0;
}

static int initQjs(lua_State *L)
{
	if (isInit == 1)
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	const char *jsPath = luaL_checkstring(L, 1);
	const char *jsFuncName = luaL_checkstring(L, 2);
	const char *configPath = luaL_checkstring(L, 3);

	int ok = initQuickjs(jsPath, jsFuncName, configPath);
	if (ok == 0)
	{
		lua_pushboolean(L, 1);
	}
	else
	{
		lua_pushboolean(L, 0);
	}

	return 1;
}

static int execQjs(lua_State *L)
{
	if(isInit != 1)
	{
		lua_pushstring(L, "not init");
		return 1;
	}

	const char *cmd = luaL_checkstring(L, 1);
	const char * args = luaL_checkstring(L, 2);
	JSValue res = execJs(cmd, args);
	const char *ret = JS_ToCString(ctx, res);
	lua_pushstring(L, ret);
	JS_FreeValue(ctx, res);
	return 1;
}

static int closeQjs(lua_State *L)
{
	cleanQuickjs();
	return 1;
}

int luaopen_lqjs(lua_State *L)
{

    luaL_Reg l[] = {
        { "init", initQjs },
        { "exec", execQjs },
        { "close", closeQjs },
        { NULL,  NULL },
    };
    luaL_newlib(L,l);
    return 1;
}
