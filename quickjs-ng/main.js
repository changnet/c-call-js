import { add } from './modules/math.js';
import { log } from './modules/utils.js';

import * as os from "os"; // not import { os } from 'qjs:os';

// 全局函数
globalThis.testFunction = function(num, str) {
	log(`platform is ${os.platform}, now is ${os.now()}`);
    log(`Called with ${num} and ${str}`);
    return add(num, 10);
};

// 使用module模式加载，直接定义的函数不会成为global，因此在C是调用不到的
function testFunction(num, str) {
    log(`Called with ${num} and ${str}`);
    return add(num, 10);
}
