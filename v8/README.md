1. 按BUILD_V8.md构建好v8库
2. 安装好lua-devel
3. cd src
4. 按v8.cpp底下的g++命令编译出v8.so，然后把头文件和库都复制到v8目录下。结构如下
```
v8/
  include/
    v8.h
    v8-*.h
  lib/
    v8_monolith.a
    v8_monolith.lib
src/
  v8.cpp
```
5. 执行测试`lua test.lua`

