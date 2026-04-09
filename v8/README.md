# 官方文档
https://v8.dev/docs/embed

# 源码
V8’s Git repository is located at https://chromium.googlesource.com/v8/v8.git, with an official mirror on GitHub: https://github.com/v8/v8

v8是滚动更新，没有stable或者lts的版本发布。因此这里采用跟随nodejs版本的规则。在[https://nodejs.org/en/about/previous-releases]中，最新的lts nodejs为24，采用的是v8版本为v13.6.233.17

https://github.com/v8/v8/tags?after=13.9.107

https://codeload.github.com/v8/v8/zip/refs/tags/13.6.233.17

https://github.com/v8/v8/archive/refs/tags/13.6.233.17-pgo.zip

pgo版本是有特殊优化的版本（PGO = Profile-Guided Optimization（基于性能分析的优化）），构建时需要有预先训练生成的负载文件.profraw 或 .gcda 等 profile 数据文件

但是注意v8官方从未提供任何从单独的源码包构建的方式，所以这个源码包是没用的，只是找出自己要的版本就可以。

# Linux编译
当前系统为Rocky Linux 10

```bash
# 安装基础依赖（需要git和python3）
sudo dnf install -y git python3 clang llvm lld gcc-c++ make cmake pkg-config glib2-devel

# 创建构建目录
mk dir v8-build
cd v8-build

# 获取depot_tools，包含对应版本的gn和ninja
# 官方教程：https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

# 临时把depot_tools放到path(用到gn和ninja)
export PATH="$HOME/v8-build/depot_tools:$PATH"

# 获取源码（这一步需要代理，大概要下载2G）
mkdir v8
fetch v8

# 切换版本
cd v8
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
# use_sysroot = false 默认v8会在依赖用使用debian_bullseye的库作为sysroot，这里不用它的，用自己系统自带的
# v8/build/linux/debian_bullseye_i386-sysroot
# use_custom_libcxx = false 使用系统的libstdc++，而不是v8自己下载的库，保证和系统上其他程序链接时兼容
# treat_warnings_as_errors = false 自己系统的gcc 14版本比较高，编译较老的版本时会有警告，默认是警告作为错误处理，这里去掉
# v8_enable_sandbox由于当前项目运行的代码保证无恶意代码，不需要启用沙盒隔离
# v8_enable_pointer_compression可以节省内存

mkdir -p out.gn/x64.release

cat > out.gn/x64.release/args.gn <<EOF
is_debug = false
target_cpu = "x64"
v8_monolithic = true
v8_static_library = true
v8_use_external_startup_data = false
is_component_build = false
is_clang = true
use_sysroot = false
use_custom_libcxx = false
clang_use_chrome_plugins = false
v8_enable_i18n_support = false
treat_warnings_as_errors = false
v8_enable_sandbox = false
v8_enable_pointer_compression = true  # 建议保留压缩，性能更好
EOF

gn gen out.gn/x64.release
ninja -C out.gn/x64.release v8_monolith

# 编译成功后测试
# v8是用clang编译的，G++程序链接必须使用-fuse-ld=lld
# V8_COMPRESS_POINTERS 和 V8_ENABLE_SANDBOX这两个宏必须要保证和上面编译时v8_enable_sandbox和v8_enable_pointer_compression一致
g++ -I. -Iinclude samples/hello-world.cc -o hello_world -fno-rtti -fuse-ld=lld -lv8_monolith -latomic -ldl -Lout.gn/x64.release/obj/ -pthread -std=c++20 -DV8_COMPRESS_POINTERS
./hello_world
```

# win编译
下面的脚本在power shell运行，在bat运行的话是不一样的。比如设置环境变量那些

v8没有在win下编译的文档，可参考chrome:https://chromium.googlesource.com/chromium/src/+/master/docs/windows_build_instructions.md#Setting-up-Windows



* 先安装python3 git Visual Studio LLVM
我的项目需要LLVM编译，你如果不用就不需要安装

Visual Studio至少要2022以上，安装时记得同时安装windows SDK。如果之前已经安装了Visual Studio，则运行Visual Sutdio Installer，点击修改，切换到单个组件，找到Windows SDK装就行。SDK版本应该>= windows 10 SDK

SDK的版本不是随便装的，要看下面SDK_VERSION对应的版本，或者高一些地最好

* 创建构建目录
```
mkdir v8-build
cd v8-build
```

* 同linux，先下载depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

* 添加到power shell的path
```powershell
$env:PATH += ";D:\dev\lua-v8\depot_tools"
```

* 获取源码(为了偷懒，我直接从linux复制整个目录过来)
```
mkdir v8
fetch v8

cd v8
git reset --hard HEAD
git checkout 13.6.233.17
```

* 还原版本（由于是从linux复制过来的，文件权限完全不一样，这里还原所有子目录）
如果是直接fetch v8则不用
```powershell
# 获取当前目录下的所有直接子目录
$directories = Get-ChildItem -Directory -Recurse

foreach ($dir in $directories) {
    $gitDir = Join-Path $dir.FullName ".git"

    # 检查是否存在 .git 文件或目录（支持常规仓库和工作树）
    if (Test-Path $gitDir) {
        Write-Host "🔧 Processing Git repository: $($dir.Name)" -ForegroundColor Cyan

        Push-Location $dir.FullName

        try {
            # 执行 git 命令
            & git reset --hard
            & git config core.filemode false

            Write-Host "Successfully reset and configured $($dir.Name)" -ForegroundColor Green
        }
        catch {
            Write-Host "Failed in $($dir.Name): $_" -ForegroundColor Red
        }
        finally {
            Pop-Location
        }
    }
    else {
        Write-Host "Skipping non-Git directory: $($dir.Name)" -ForegroundColor Gray
    }
}
```

* 设置不下载google的tool chain
否则会报No downloadable toolchain found. In order to use your locally installed version of Visual Studio to build Chrome please set DEPOT_TOOLS_WIN_TOOLCHAIN=0
```powershell
$env:DEPOT_TOOLS_WIN_TOOLCHAIN=0
```

* 同步依赖(这一步必须得执行，linux复制过来的可以减少下载，但得重新同步win相关的)
gclient sync

# 手动创建编译目录
mkdir -p out.gn/x64.release

* 手动创建out.gn/x64.release/args.gn，写入以下内容
```
is_debug = false
target_cpu = "x64"
v8_monolithic = true
v8_static_library = true
v8_use_external_startup_data = false
is_component_build = false
is_clang = true
clang_base_path = "D:/LLVM17"
clang_version = "17"
use_custom_libcxx = false
clang_use_chrome_plugins = false
v8_enable_i18n_support = false
treat_warnings_as_errors = false
v8_enable_sandbox = false
v8_enable_pointer_compression = true
use_lld = true
is_official_build = false
llvm_force_head_revision = false
```


* 手动修改配置文件
1. 修改v8\build\toolchain\win\setup_toolchain.py中的SDK_VERSION为自己安装的版本，比如'10.0.22621.0'。它默认10.0.26100.0且无法通过配置、环境变量修改。26100是win11的，在win10的安装器上根本看不到这个版本

2. v8\build\vs_toolchain.py中GetVisualStudioVersion()直接return "2022"，因为它无法检测我本地的vs版本

3. v8\build\vs_toolchain.py中DetectVisualStudioPath()直接返回return r'D:\Program Files\Microsoft Visual Studio\2022\Community'。如果你安装的是默认路径，也许不需要修改，你可以尝试先执行gn gen，报错再修改

* 修改编译参数
由于我用的llvm为17，版本较低，要在compiler\BUILD.gn中找到下面的字符串，去掉
```
-Wno-nontrivial-memcall
-split-threshold-for-reg-with-hint
-Wno-thread-safety-reference-return
-Wno-c++11-narrowing-const-reference
```


由于我改低了SDK的版本（NTDDI_WIN11_GE是>=WIN 11，而我在win10的SDK上编译），编译时会提示
```
Windows Kits/10/include/10.0.22621.0/um\winbase.h(9258,11): error: unknown type name 'FILE_INFO_BY_HANDLE_CLASS'
 9258 |     _In_  FILE_INFO_BY_HANDLE_CLASS FileInformationClass
```
修改BUILD.gn
```
  defines = [
    "NTDDI_VERSION=NTDDI_WIN11_GE",

    # We can't say `=_WIN32_WINNT_WIN10` here because some files do
    # `#if WINVER < 0x0600` without including windows.h before,
    # and then _WIN32_WINNT_WIN10 isn't yet known to be 0x0A00.
    "_WIN32_WINNT=0x0A00",
    "WINVER=0x0A00",
  ]
```
修改后
```
  defines = [
    "NTDDI_VERSION=0x0A00000C",

    # We can't say `=_WIN32_WINNT_WIN10` here because some files do
    # `#if WINVER < 0x0600` without including windows.h before,
    # and then _WIN32_WINNT_WIN10 isn't yet known to be 0x0A00.
    "_WIN32_WINNT=0x0A00",
    "WINVER=0x0A00",
  ]
```

LLVM不能安装到带空格的路径，比如“Program Files”，否则会出现下面的错误
```
FAILED: bytecode_builtins_list_generator.exe bytecode_builtins_list_generator.exe.pdb
..\..\..\..\..\Program Files\LLVM\bin\lld-link.exe "/OUT:./bytecode_builtins_list_generator.exe" /nologo -libpath:../../../../../Program Files/LLVM/lib/clang/17/lib/windows "-libpath:../../../../../Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.40.33807/ATLMFC/lib/x64" "-libpath:../../../../../Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.40.33807/lib/x64" "-libpath:C:\Program Files (x86)\Windows Kits\NETFXSDK\4.8\lib\um\x64" "-libpath:../../../../../Windows Kits/10/lib/10.0.22621.0/ucrt/x64" "-libpath:../../../../../Windows Kits/10/lib/10.0.22621.0/um/x64" /MACHINE:X64  "/PDB:./bytecode_builtins_list_generator.exe.pdb" "@./bytecode_builtins_list_generator.exe.rsp"
lld-link: error: could not open 'Files/LLVM/lib/clang/17/lib/windows': no such file or directory
```
已经安装的可以在powershell中创建一个硬链接`New-Item -ItemType Junction -Path "D:\LLVM17" -Target "D:\Program Files\LLVM"`

* 开始编译
```
gn gen out.gn/x64.release
ninja -C out.gn/x64.release v8_monolith
```

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

4. https://blog.j2i.net/2025/05/06/compiling-v8-on-windows-version-13-7-9/
https://gist.github.com/jhalon/5cbaab99dccadbf8e783921358020159


# 嵌套nodejs
C++要运行js代码，除了v8还可以直接嵌套nodejs。nodejs基于v8的同时还提供了大量的接口，比如文件读写、libuv等，但也重度得多。

https://nodejs.org/api/embedding.html

quickjs也可以，但这个没有jit，和v8比太慢了。

