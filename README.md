# ScyllaX

> ScyllaX 是基于原版 Scylla 的 PE dump / imports reconstruction 工具改造版。当前仓库将 Scylla 的 WTL 图形界面保留下来，并新增 `ScyllaXBridgePlugin`，让 ScyllaX 通过 x64dbg/x32dbg 插件桥接当前正在调试的进程，而不是由 ScyllaX 自己枚举进程并 `OpenProcess` 读取目标内存。

[![绿色免安装下载](https://img.shields.io/badge/Download-Latest_Release-blue?style=for-the-badge)](https://github.com/Project-HashCat/ScyllaX/releases/latest)

## 特性

- 支持 x86 / x64 PE 目标。
- 保留原 Scylla 的经典 UI 和主要工作流：
  - Dump 当前目标映像；
  - IAT Autosearch；
  - Get Imports；
  - Fix Dump；
  - PE Rebuild；
  - import tree 保存 / 加载；
  - invalid / suspect imports 检查；
  - direct imports 扫描与修复选项。
- 新增 x64dbg / x32dbg Bridge 模式：
  - `ScyllaX.exe` 不再直接打开目标进程；
  - 目标进程、模块列表、内存读写、内存区域查询均通过命名管道转发到 debugger 插件；
  - 插件侧使用 x64dbg SDK / Script API 访问当前 debuggee；
  - OEP 默认使用 debugger 当前 `cip`。
- 支持 x64dbg 插件菜单一键启动：
  - `Plugins -> ScyllaXBridge -> Open ScyllaX UI`
- 提供 GitHub Actions 构建配置，可自动补依赖、下载 x64dbg pluginsdk 并构建 x86/x64 artifact。



## 工作原理

```txt
ScyllaX.exe UI
  -> named pipe
  -> ScyllaXBridgePlugin.dp64 / ScyllaXBridgePlugin.dp32
  -> x64dbg / x32dbg SDK
  -> current debuggee
```

ScyllaX 启动后会解析命令行参数：

```txt
--xdbg-pipe "\\.\pipe\ScyllaXBridge_xxx_xxx"
```

如果没有这个参数，程序会直接退出并提示必须从 x64dbg/x32dbg 插件启动。

Bridge 当前实现的命令：

| 命令                    | 作用                     |
| ----------------------- | ------------------------ |
| `XDBG_CMD_HELLO`        | 握手测试                 |
| `XDBG_CMD_GET_PROCESS`  | 获取当前 debuggee 信息   |
| `XDBG_CMD_GET_MODULES`  | 获取模块列表             |
| `XDBG_CMD_READ_MEMORY`  | 从当前 debuggee 读取内存 |
| `XDBG_CMD_WRITE_MEMORY` | 向当前 debuggee 写内存   |
| `XDBG_CMD_QUERY_MEMORY` | 查询地址所属内存区域     |

## 目录结构

```txt
ScyllaX-master/
├─ Scylla.sln                         Visual Studio solution
├─ Scylla/                             Scylla UI 与 PE/IAT 核心逻辑
│  ├─ main.cpp                         GUI 入口，当前为 bridge-only 启动
│  ├─ MainGui.cpp/.h                   主窗口逻辑
│  ├─ XDbgBridge.cpp/.h                ScyllaX 侧命名管道 client
│  ├─ ProcessAccessHelp.cpp/.h         进程/内存访问封装，bridge 模式下转发到 xdbg
│  ├─ ProcessLister.cpp/.h             进程列表封装，bridge 模式下只返回当前 debuggee
│  ├─ IATSearch.cpp/.h                 IAT 自动搜索
│  ├─ ImportsHandling.cpp/.h           imports tree 解析与处理
│  ├─ ImportRebuilder.cpp/.h           import table 重建
│  ├─ PeParser.cpp/.h                  PE 解析
│  └─ PeRebuild.cpp/.h                 PE rebuild
├─ ScyllaXBridgePlugin/                x64dbg/x32dbg 插件 bridge server
│  ├─ ScyllaXBridgePlugin.cpp          插件入口、菜单、pipe server、xdbg API 调用
│  ├─ plugin.h
│  ├─ pluginmain.h
│  └─ ScyllaXBridgePlugin.vcxproj
├─ diStorm/                            diStorm 反汇编库工程
├─ tinyxml/                            TinyXML 工程；源码需补齐
├─ WTL/                                WTL 目录；当前仓库中通常需作为 submodule/依赖补齐
├─ Plugins/                            原 Scylla 插件与 ImpREC wrapper 示例
├─ .github/workflows/build.yml         GitHub Actions 构建流程
├─ deploy_example.bat                  部署示例脚本
├─ ScyllaX.ini                         默认配置示例
├─ README_BRIDGE_ONLY.md               bridge-only 行为说明
├─ README_ScyllaX.md                   ScyllaX MVP 说明
├─ COMPILING                           原版 Scylla 编译说明
└─ LICENSE                             GPL-3.0 license
```
## 系统要求

- Windows 10 / Windows 11, Windows 7 可用, 但是不建议.
- 有x96dbg


## 编译环境

### 必需

- Windows 10 / Windows 11，Windows 7 理论上也可能可用，但建议使用较新的 Windows。
- x64dbg / x32dbg。
- Visual Studio 2022，或可用的 MSBuild + MSVC toolchain。
- Windows SDK。
- WTL。
- TinyXML legacy 版本。
- x64dbg pluginsdk。

### 架构匹配

| 目标程序 | Debugger | Bridge 插件                | ScyllaX           |
| -------- | -------- | -------------------------- | ----------------- |
| x64 PE   | x64dbg   | `ScyllaXBridgePlugin.dp64` | x64 `ScyllaX.exe` |
| x86 PE   | x32dbg   | `ScyllaXBridgePlugin.dp32` | x86 `ScyllaX.exe` |

不要混用架构。x64dbg 只能配 `.dp64`，x32dbg 只能配 `.dp32`。

## 获取依赖

### 1. WTL

仓库里有 `.gitmodules`：

```ini
[submodule "WTL"]
    path = WTL
    url = https://github.com/Win32-WTL/WTL.git
```
使用:

```bat
git submodule update --init --recursive
```

最终至少需要存在：

```txt
WTL\Include\atlapp.h
```


### 2. TinyXML

原 Scylla 使用的是 legacy TinyXML，不是 tinyxml2。当前仓库的 `tinyxml/` 里可能只有工程文件，源码需要补齐。

可以这样处理：

```bat
git clone --depth 1 https://github.com/lucasg/tinyxml.git _deps\tinyxml
copy _deps\tinyxml\tiny*.cpp tinyxml\
copy _deps\tinyxml\tiny*.h tinyxml\
```

最终建议至少有：

```txt
tinyxml\tinyxml.cpp
tinyxml\tinyxml.h
tinyxml\tinystr.cpp
tinyxml\tinystr.h
tinyxml\tinyxmlerror.cpp
tinyxml\tinyxmlparser.cpp
```

### 3. x64dbg pluginsdk

`ScyllaXBridgePlugin` 需要 x64dbg 官方 plugin SDK。放置结构如下：

```txt
ScyllaXBridgePlugin\
├─ pluginsdk\
│  ├─ bridgemain.h
│  ├─ _plugins.h
│  ├─ _scriptapi_module.h
│  ├─ _scriptapi_memory.h
│  ├─ x64dbg.lib
│  ├─ x64bridge.lib
│  ├─ x32dbg.lib
│  └─ x32bridge.lib
├─ ScyllaXBridgePlugin.cpp
├─ plugin.h
└─ pluginmain.h
```

x64dbg snapshot 或 `x64dbg-pluginsdk.zip` 一般都包含这些文件。

## 编译

### 方式一：Visual Studio 2022

1. 补齐 `WTL`、`tinyxml`、`ScyllaXBridgePlugin/pluginsdk`。
2. 打开：

```txt
Scylla.sln
```

3. 如果 Visual Studio 提示 retarget，选择当前机器安装的 Windows SDK 和 `v143` toolset。
4. 构建：

```txt
Release | x64
Release | Win32
```

通常会生成：

```txt
x64\Release\ScyllaX.exe
Win32\Release\ScyllaX.exe
bin\x64\ScyllaXBridgePlugin.dp64
bin\x32\ScyllaXBridgePlugin.dp32
```

> 注意：原 Scylla 工程比较老，Win32 配置里可能写着 `v90`。没有 VS2008 时，直接 retarget 到 `v143` 即可。

### 方式二：命令行 MSBuild

x64：

```bat
msbuild Scylla.sln /m /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143
```

x86：

```bat
msbuild Scylla.sln /m /restore /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143
```

Debug 构建：

```bat
msbuild Scylla.sln /m /restore /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v143
msbuild Scylla.sln /m /restore /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v143
```

## GitHub Actions 构建

仓库内置 `.github/workflows/build.yml`。它会：

1. checkout 仓库与 submodule；
2. 设置 VS2022 MSBuild；
3. 检查并补齐 WTL；
4. 检查并补齐 TinyXML；
5. 下载 x64dbg pluginsdk；
6. 将旧工程 retarget 到 `v143`；
7. 分别构建 `x64` 与 `Win32`；
8. 上传 `ScyllaX-x64` 和 `ScyllaX-x86` artifacts。

手动触发路径：

```txt
GitHub -> Actions -> Build ScyllaX -> Run workflow
```

## 部署

### x64dbg

把 64 位插件和 64 位 ScyllaX 放到 x64dbg 插件目录：

```txt
x64dbg\release\x64\plugins\ScyllaXBridgePlugin.dp64
x64dbg\release\x64\plugins\ScyllaX.exe
```

也可以把 `ScyllaX.exe` 放到 `x64dbg.exe` 同级目录。插件会先找自身目录，再 fallback 到父目录。

### x32dbg

把 32 位插件和 32 位 ScyllaX 放到 x32dbg 插件目录：

```txt
x64dbg\release\x32\plugins\ScyllaXBridgePlugin.dp32
x64dbg\release\x32\plugins\ScyllaX.exe
```

也可以把 `ScyllaX.exe` 放到 `x32dbg.exe` 同级目录。

### 使用部署脚本

仓库提供 `deploy_example.bat`：

```bat
deploy_example.bat C:\Tools\x64dbg\release\x64 x64
deploy_example.bat C:\Tools\x64dbg\release\x32 x32
```

脚本会复制：

```txt
bin\x64\ScyllaXBridgePlugin.dp64 -> x64dbg\release\x64\plugins\
bin\x32\ScyllaXBridgePlugin.dp32 -> x64dbg\release\x32\plugins\
```

并尝试复制对应架构的 `ScyllaX.exe`。如果你的 `ScyllaX.exe` 输出在 `x64\Release\` 或 `Win32\Release\`，请按实际输出目录调整脚本。

## 使用方法

### 基本流程

1. 打开 x64dbg 或 x32dbg。
2. 载入你有权限分析的目标程序。
3. 运行或单步到你认为合适的 OEP / unpacked code 附近。
4. 点击菜单：

```txt
Plugins -> ScyllaXBridge -> Open ScyllaX UI
```

5. ScyllaX 窗口启动后会自动连接当前 debuggee。
6. 进程下拉框会显示当前 debuggee，但 bridge-only 版不允许自由选择其他进程。
7. OEP 默认填入 x64dbg/x32dbg 当前 `cip`。
8. 根据需要执行：

```txt
IAT Autosearch -> Get Imports -> Dump -> Fix Dump
```

或：

```txt
Dump -> PE Rebuild
```

### IAT 修复典型流程

1. 在 debugger 中停在 OEP 或接近 OEP 的位置。
2. 打开 ScyllaX UI。
3. 确认 OEP 值是否正确。
4. 点击 `IAT Autosearch` 自动搜索 IAT 起始地址和大小。
5. 点击 `Get Imports` 解析导入表。
6. 查看 imports tree：
  - `Show Invalid`：显示解析失败项；
  - `Show Suspect`：显示可疑项；
  - 右键可编辑、删除或手动选择 API。
7. 点击 `Dump` 导出当前映像。
8. 点击 `Fix Dump` 将重建后的 import table 写入 dump 文件。
9. 用 PE 工具或 debugger 验证修复后的文件是否能正常装载。

### PE Rebuild

`PE Rebuild` 用于对已有 dump 文件做 PE 结构修复。适合在 dump 后进一步清理 header、section、checksum 等信息。

### Dump memory / Dump section

Scylla 原有的内存区域 dump 和 PE section dump 界面仍保留。bridge 模式下，底层读取会通过 x64dbg/x32dbg 完成。

## 配置文件

仓库提供 `ScyllaX.ini` 示例。常见配置项：

```ini
[SCYLLA_CONFIG]
USE_PE_HEADER_FROM_DISK=1
DEBUG_PRIVILEGE=1
CREATE_BACKUP=1
DLL_INJECTION_AUTO_UNLOAD=0
IAT_SECTION_NAME=.SCY
UPDATE_HEADER_CHECKSUM=1
REMOVE_DOS_HEADER_STUB=0
IAT_FIX_AND_OEP_FIX=1
SUSPEND_PROCESS_FOR_DUMPING=0
OriginalFirstThunk_SUPPORT=1
USE_ADVANCED_IAT_SEARCH=1
SCAN_DIRECT_IMPORTS=1
FIX_DIRECT_IMPORTS_NORMAL=0
FIX_DIRECT_IMPORTS_UNIVERSAL=1
CREATE_NEW_IAT_IN_SECTION=0
DONT_CREATE_NEW_SECTION=0
APIS_ALWAYS_FROM_DISK=1
```

建议 bridge-only 版保持：

```ini
SUSPEND_PROCESS_FOR_DUMPING=0
APIS_ALWAYS_FROM_DISK=1
USE_ADVANCED_IAT_SEARCH=1
```

说明：

| 配置项                         | 说明                                                   |
| ------------------------------ | ------------------------------------------------------ |
| `USE_PE_HEADER_FROM_DISK`      | 优先使用磁盘 PE header。目标内存 header 被改写时有用。 |
| `CREATE_BACKUP`                | 修改文件前创建备份。                                   |
| `IAT_SECTION_NAME`             | 新建 IAT section 的名称。                              |
| `UPDATE_HEADER_CHECKSUM`       | 修复后更新 PE checksum。                               |
| `IAT_FIX_AND_OEP_FIX`          | Fix Dump 时同时修复 IAT 与 OEP。                       |
| `OriginalFirstThunk_SUPPORT`   | 启用 OriginalFirstThunk 支持。                         |
| `USE_ADVANCED_IAT_SEARCH`      | 使用高级 IAT 搜索算法。                                |
| `SCAN_DIRECT_IMPORTS`          | 扫描 direct imports。                                  |
| `FIX_DIRECT_IMPORTS_UNIVERSAL` | 使用 universal 方式修复 direct imports。               |
| `APIS_ALWAYS_FROM_DISK`        | API 解析时始终从磁盘模块读取导出信息。                 |

## 快捷键

| 快捷键     | 功能           |
| ---------- | -------------- |
| `Ctrl + D` | Dump           |
| `Ctrl + F` | Fix Dump       |
| `Ctrl + R` | PE Rebuild     |
| `Ctrl + O` | Load Tree      |
| `Ctrl + S` | Save Tree      |
| `Ctrl + T` | Autotrace      |
| `Ctrl + G` | Get Imports    |
| `Ctrl + I` | IAT Autosearch |

## 插件系统

仓库保留原 Scylla 插件目录：

```txt
Plugins\
├─ PECompact.dll
├─ PESpin_x64_v1.dll
├─ Include_Headers\ScyllaPlugin.h
├─ ImpRec_Plugins\Imprec_Wrapper_DLL.dll
├─ ImpRec_Plugins\PECompact 2.7.x.dll
└─ Sources\
```

原 Scylla 支持：

- Scylla native plugin；
- 部分 ImpREC plugin wrapper。

Bridge-only 版仍会扫描插件目录，但涉及远程线程、DLL 注入或原生进程句柄的旧插件逻辑可能并未完全适配 xdbg bridge。若插件行为异常，请优先确认该插件是否依赖原版 `OpenProcess` / `CreateRemoteThread` 流程。

## 已知限制

- 不支持在 ScyllaX UI 内自由选择系统进程。
- `Pick DLL` 无法使用
- `Suspend / Resume / Terminate` 类操作应交给 x64dbg/x32dbg 控制。
- `QUERY_MEMORY` 当前主要通过 `DbgMemFindBaseAddr` 得到 base / size，`Protect` / `Type` 信息不完整。
- 插件一次只处理一个 ScyllaX UI 连接。
- 一些 legacy Scylla 功能保留在代码里，但当前工程主要目标是 xdbg bridge workflow。


## 常见问题


### 1. x64dbg 菜单里看不到 ScyllaXBridge

检查文件是否放对：

```txt
x64dbg\release\x64\plugins\ScyllaXBridgePlugin.dp64
```

或：

```txt
x64dbg\release\x32\plugins\ScyllaXBridgePlugin.dp32
```

然后重启 x64dbg/x32dbg。

### 2. 插件提示找不到 ScyllaX.exe

把 `ScyllaX.exe` 放到以下任一位置：

```txt
x64dbg\release\x64\plugins\ScyllaX.exe
x64dbg\release\x64\ScyllaX.exe
```

x32dbg 同理：

```txt
x64dbg\release\x32\plugins\ScyllaX.exe
x64dbg\release\x32\ScyllaX.exe
```

### 3. IAT Autosearch 找不到 IAT

可以尝试：

- 确认 debugger 停在解包后的 OEP 附近；
- 手动填写更合适的 OEP；
- 开启 `USE_ADVANCED_IAT_SEARCH=1`；
- 确认目标模块列表是否正常；
- 对可疑 imports 使用手动 Pick API 修正。

### 4. Get Imports 有 invalid / suspect 项

这在壳、混淆或跳板导入场景里很常见。处理方式：

- 用 `Show Invalid` / `Show Suspect` 过滤；
- 删除明显错误项；
- 对无法自动识别的 thunk 手动选择 DLL/API；
- 必要时扩大或缩小 IAT 范围后重新 `Get Imports`。

## 开发说明

### ScyllaX 侧

核心文件：

```txt
Scylla\XDbgBridge.h
Scylla\XDbgBridge.cpp
```

主要职责：

- 解析 `--xdbg-pipe` 参数；
- 连接命名管道；
- 发送 request packet；
- 接收 response payload；
- 封装 process / module / memory API；
- 将大块读写按 `0x100000` 分块。

`ProcessAccessHelp.cpp` 中的关键改造：

- bridge 模式下 `openProcessHandle` 使用 sentinel handle；
- `readMemoryFromProcess` 转发到 `XDBG_CMD_READ_MEMORY`；
- `writeMemoryToProcess` 转发到 `XDBG_CMD_WRITE_MEMORY`；
- `getProcessModules` 转发到 `XDBG_CMD_GET_MODULES`；
- `getMemoryRegionFromAddress` 转发到 `XDBG_CMD_QUERY_MEMORY`。

### x64dbg/x32dbg 插件侧

核心文件：

```txt
ScyllaXBridgePlugin\ScyllaXBridgePlugin.cpp
```

主要职责：

- 注册 x64dbg 插件；
- 添加菜单项 `Open ScyllaX UI`；
- 创建命名管道；
- 启动 `ScyllaX.exe --xdbg-pipe ...`；
- 接收 ScyllaX 请求；
- 调用 x64dbg SDK / Script API；
- 返回二进制 payload。

插件入口：

```cpp
pluginit
plugstop
plugsetup
CBMENUENTRY
DllMain
```

当前主要使用的 xdbg API：

```cpp
DbgIsDebugging()
DbgGetProcessId()
DbgValFromString("cip")
DbgMemRead()
DbgMemFindBaseAddr()
Script::Memory::Write()
Script::Module::GetMainModuleInfo()
Script::Module::GetList()
```

### Pipe packet

客户端和插件端共用结构：

```cpp
#define SCYLLAX_PIPE_MAGIC 0x58594353u /* SCYX */
#define SCYLLAX_PIPE_VERSION 1u

struct XDbgPacket
{
    DWORD magic;
    DWORD version;
    DWORD command;
    DWORD status;
    unsigned long long a;
    unsigned long long b;
    unsigned long long c;
    unsigned long long d;
    DWORD size;
};
```

payload 紧跟在 packet 后面。reply 的 `status = 1` 表示成功，`status = 0` 表示失败。

## 代码风格建议

- 保持 C++ 代码与原 Scylla 风格一致，尽量少引入大型新依赖。
- 新增 bridge 命令时，需要同时修改：
  - `Scylla/XDbgBridge.h`
  - `Scylla/XDbgBridge.cpp`
  - `ScyllaXBridgePlugin/ScyllaXBridgePlugin.cpp`
- 修改 wire struct 时注意 x86/x64 ABI、字段大小和对齐。
- pipe payload 尽量使用固定大小结构，避免 STL 对象跨进程传输。
- 涉及目标进程控制的操作优先交给 x64dbg/x32dbg，不要恢复原生 `OpenProcess` fallback。

## 安全与合法使用

ScyllaX 是调试和 PE 修复工具。请只用于：

- 自己开发的软件；
- CTF / crackme / unpackme；
- 已获授权的逆向分析；
- 恶意样本的隔离研究环境；
- PE 文件格式学习。

不要将本工具用于未授权的软件修改、绕过授权、破坏系统、窃取数据或其他违法行为。

## License

本项目继承原 Scylla 许可，使用 **GNU General Public License v3.0**。详见：

```txt
LICENSE
```

## Credits

- Original Scylla project：x86/x64 imports reconstruction tool。
- diStorm：反汇编引擎。
- TinyXML：XML 配置解析。
- WTL：Windows Template Library UI。
- x64dbg / x32dbg：debugger 与插件 SDK。
- ChatGPT 5.5: 太好用了(
- BennettNotFound: ScyllaX作者(Project HashCat)
- 以及屏幕前的你~