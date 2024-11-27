
print("start test lua quickjs-ng")

local lqjs = require('lqjs')

local res = lqjs.init('test.js', 'exec', 'config')
assert(res)
local ret1 = lqjs.exec('RUN', "run args1");
print('------------', ret1)


local ret2 = lqjs.exec('RUN', "run args2");
print('------------', ret2)
lqjs.close()
