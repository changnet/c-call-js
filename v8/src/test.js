// test.js - 被 Lua 通过 V8 调用的 JS 测试代码

/**
 * exec(args) - 主入口函数
 * args 是从 Lua 传入的 JSON 对象: { cmd: "INIT", data: "..." }
 * 返回值会被 JSON.stringify 后传回 Lua
 */
function exec(args) {
    if (typeof args === 'string') {
        // 兼容字符串参数
        return 'echo: ' + args;
    }

    var cmd = args.cmd || '';
    var data = args.data || '';

    switch (cmd) {
        case 'INIT':
            return { status: 'ok', message: 'initialized', data: data };
        case 'ADD':
            // 简单的加法运算 { cmd: "ADD", data: { a: 1, b: 2 } }
            var a = (args.data && args.data.a) || 0;
            var b = (args.data && args.data.b) || 0;
            return { status: 'ok', result: a + b };
        case 'UPPER':
            return { status: 'ok', result: data.toUpperCase() };
        case 'EVAL':
            // 动态执行表达式
            try {
                var result = eval(data);
                return { status: 'ok', result: result };
            } catch (e) {
                return { status: 'error', message: e.toString() };
            }
        default:
            return { status: 'error', message: 'unknown cmd: ' + cmd };
    }
}

/**
 * add(args) - 加法函数
 * args: { a: number, b: number }
 */
function add(args) {
    return { result: (args.a || 0) + (args.b || 0) };
}

/**
 * hello(args) - 返回问候语
 * args: { name: string }
 */
function hello(args) {
    var name = (args && args.name) || 'World';
    return 'Hello, ' + name + '!';
}

/**
 * fibonacci(args) - 计算斐波那契数列
 * args: { n: number }
 */
function fibonacci(args) {
    var n = (args && args.n) || 0;
    if (n <= 0) return { result: 0 };
    if (n === 1) return { result: 1 };
    var a = 0, b = 1;
    for (var i = 2; i <= n; i++) {
        var temp = a + b;
        a = b;
        b = temp;
    }
    return { result: b };
}
