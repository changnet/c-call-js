-- test.lua - 测试 Lua 加载 V8 库并调用 JS 代码

print("========================================")
print("  Lua V8 Binding Test")
print("========================================")

-- 加载 v8 库
local v8 = require('v8')

-- 打印 V8 版本
print("[INFO] V8 version: " .. v8.version())

-- 初始化 V8 并加载JS脚本
print("\n[TEST 1] v8.init()")
local ok, err = v8.init("test.js")
if not ok then
    print("[FAIL] init failed: " .. tostring(err))
    os.exit(1)
end
print("[PASS] V8 initialized successfully")

-- 测试调用 exec 函数 - INIT 命令
print("\n[TEST 2] v8.call('exec', INIT)")
local result, err = v8.call("exec", '{"cmd":"INIT","data":"hello from lua"}')
if not result then
    print("[FAIL] " .. tostring(err))
else
    print("[PASS] result: " .. result)
end

-- 测试调用 exec 函数 - ADD 命令
print("\n[TEST 3] v8.call('exec', ADD)")
local result, err = v8.call("exec", '{"cmd":"ADD","data":{"a":10,"b":20}}')
if not result then
    print("[FAIL] " .. tostring(err))
else
    print("[PASS] result: " .. result)
end

-- 测试调用 exec 函数 - UPPER 命令
print("\n[TEST 4] v8.call('exec', UPPER)")
local result, err = v8.call("exec", '{"cmd":"UPPER","data":"hello world"}')
if not result then
    print("[FAIL] " .. tostring(err))
else
    print("[PASS] result: " .. result)
end

-- 测试调用 exec 函数 - EVAL 命令
print("\n[TEST 5] v8.call('exec', EVAL)")
local result, err = v8.call("exec", '{"cmd":"EVAL","data":"2 * 3 + 4"}')
if not result then
    print("[FAIL] " .. tostring(err))
else
    print("[PASS] result: " .. result)
end

-- 测试调用 add 函数
print("\n[TEST 6] v8.call('add')")
local result, err = v8.call("add", '{"a":100,"b":200}')
if not result then
    print("[FAIL] " .. tostring(err))
else
    print("[PASS] result: " .. result)
end

-- 测试调用 hello 函数
print("\n[TEST 7] v8.call('hello')")
local result, err = v8.call("hello", '{"name":"Lua"}')
if not result then
    print("[FAIL] " .. tostring(err))
else
    print("[PASS] result: " .. result)
end

-- 测试调用 fibonacci 函数
print("\n[TEST 8] v8.call('fibonacci')")
local result, err = v8.call("fibonacci", '{"n":10}')
if not result then
    print("[FAIL] " .. tostring(err))
else
    print("[PASS] result: " .. result)
end

-- 测试不存在的函数
print("\n[TEST 9] v8.call('not_exist') - 测试错误处理")
local result, err = v8.call("not_exist", '{}')
if not result then
    print("[PASS] expected error: " .. tostring(err))
else
    print("[FAIL] should have returned error")
end

-- 测试 exec 未知命令
print("\n[TEST 10] v8.call('exec', unknown cmd)")
local result, err = v8.call("exec", '{"cmd":"UNKNOWN"}')
if not result then
    print("[FAIL] " .. tostring(err))
else
    print("[PASS] result: " .. result)
end

-- 销毁 V8 环境
print("\n[TEST 11] v8.uninit()")
v8.uninit()
print("[PASS] V8 uninitialized")

-- 测试 uninit 后调用
print("\n[TEST 12] v8.call after uninit - 测试错误处理")
local result, err = v8.call("exec", '{}')
if not result then
    print("[PASS] expected error: " .. tostring(err))
else
    print("[FAIL] should have returned error")
end

print("\n========================================")
print("  All tests completed!")
print("========================================")
