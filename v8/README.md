# 官方文档
https://v8.dev/docs/embed

# 源码
V8’s Git repository is located at https://chromium.googlesource.com/v8/v8.git, with an official mirror on GitHub: https://github.com/v8/v8

v8是滚动更新，没有stable或者lts的版本发布。因此这里采用跟随nodejs版本的规则。在https://nodejs.org/en/about/previous-releases中，最新的lts nodejs为24，采用的是v8版本为v13.6.233.17

https://github.com/v8/v8/tags?after=13.9.107

https://codeload.github.com/v8/v8/zip/refs/tags/13.6.233.17
https://github.com/v8/v8/archive/refs/tags/13.6.233.17-pgo.zip

pgo版本是有特殊优化的版本（PGO = Profile-Guided Optimization（基于性能分析的优化）），构建时需要有预先训练生成的负载文件.profraw 或 .gcda 等 profile 数据文件

或者干脆点，直接去下载nodejs源码（注意要找到LTS版本的），然后去deps/v8里构建就行。https://github.com/nodejs/node/releases

但是注意v8官方从未提供任何从源码包构建的方式，所以这个源码包是没用的，只是找出自己要的版本就可以。

# Linux编译
https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up

```base
# 安装基础依赖（需要git和python3）
sudo dnf install -y git python3 clang llvm lld gcc-c++ make cmake pkg-config glib2-devel

# 创建构建目录
mk dir v8-build
cd v8-build

# 获取depot_tools，包含对应版本的gn和ninja
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

# 临时把depot_tools放到path(用到gn和ninja)
export PATH="$HOME/v8-build/depot_tools:$PATH"

# 获取源码（这一步需要代理，大概要下载2G）
mkdir v8
fetch v8

# 切换版本
git checkout 13.6.233.17

# 同步依赖（这一步需要代理，大概要下载6G）
gclient sync

# 如果发现文件缺失或者错误（通常是gclient中断过）可以用以下命令修复
# 这个并不会重新下载所有文件，会比较快
git reset --hard HEAD
git checkout 13.6.233.17
gclient sync -D --force --reset

# 构建
# 使用官方的方法（https://v8.dev/docs/build）构建不太行，因为不是最新。并且程序后面需要和gcc程序链接
# is_clang = true 使用clang构建
# use_sysroot = false 默认v8会在依赖用使用debian_bulleye的库作为sysroot，这里不用它的，用自己系统自带的
# use_custom_libcxx = false 使用系统的libstdc++，而不是v8自己下载的库，保证和系统上其他程序链接时兼容
# treat_warnings_as_errors = false 自己系统的gcc 14版本比较高，编译较老的版本时会有警告，默认是警告作为错误处理，这里去掉

mkdir -p out.gn/x64.release

cat > out.gn/x64.release/args.gn <<EOF
is_debug = false
target_cpu = "x64"
v8_monolithic = true
v8_use_external_startup_data = false
is_component_build = false
is_clang = true
use_sysroot = false
use_custom_libcxx = false
clang_use_chrome_plugins = false
v8_enable_i18n_support = false
treat_warnings_as_errors = false
EOF

gn gen out.gn/x64.release
ninja -C out.gn/x64.release v8_monolith

# 编译成功后测试
# v8是用clang编译的，G++程序链接必须使用-fuse-ld=lld
# 
g++ -I. -Iinclude samples/hello-world.cc -o hello_world -fno-rtti -fuse-ld=lld -lv8_monolith -latomic -ldl -Lout.gn/x64.release/obj/ -pthread -std=c++20 -DV8_COMPRESS_POINTERS -DV8_ENABLE_SANDBOX
./hello_world
```

# win编译

编译时，指向llvm 17
```
is_debug = false
v8_monolith = true
v8_static_library = true
is_component_build = false

# 解决 LLVM 17 兼容性的核心
use_custom_libcxx = false          # 强制使用 MSVC STL，解决 ABI 冲突
clang_base_path = "C:/Program Files/LLVM" # 指向你的 LLVM 17 路径
clang_use_chrome_plugins = false    # 禁用 Google 专用插件
treat_warnings_as_errors = false   # 忽略版本差异导致的警告
```

https://blog.j2i.net/2025/05/06/compiling-v8-on-windows-version-13-7-9/
https://gist.github.com/jhalon/5cbaab99dccadbf8e783921358020159

# 其他构建参考
1. https://github.com/jeroen/build-v8-static/blob/master/rocky-8/Dockerfile
2. nodejs的源码有个构建脚本：https://github.com/nodejs/node/blob/main/tools/make-v8.sh
理论上这个比v8官方的构建系统更简单，并且它的源码里直接有third_party的文件，不需要gclent sync等
```
# 下载nodejs lts源码包并解压
cd node-24.14.1
./configure
bash tools/make-v8.sh
```
3. https://github.com/bnoordhuis/v8-cmake


# 嵌套nodejs
C++要运行js代码，除了v8还可以直接嵌套nodejs。nodejs基于v8的同时还提供了大量的接口，比如文件读写、libuv等，但也重度得多。

https://nodejs.org/api/embedding.html

quickjs也可以，但这个没有jit，和v8比太慢了。

