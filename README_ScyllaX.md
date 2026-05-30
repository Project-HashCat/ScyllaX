# ScyllaX MVP

这个包是在你上传的 `Scylla-master` 基础上改的一个 MVP：

- `Scylla` 项目保留原 Scylla/WTL UI，窗口名改成 `ScyllaX`。
- 新增 `Scylla/XDbgBridge.h/.cpp`。
- 新增 `ScyllaXBridgePlugin` x64dbg/x32dbg 插件项目。
- ScyllaX.exe 不再直接用 `OpenProcess + ReadProcessMemory` 读目标进程；在 `--xdbg-pipe` 模式下，读内存、写内存、模块枚举、内存区域查询都走命名管道到 x64dbg 插件。
- 插件再调用 x64dbg SDK：`Script::Memory::Read/Write`、`Script::Module::GetList`、`DbgMemFindBaseAddr`、`DbgValFromString("cip")` 等。

## 目录

```txt
ScyllaX_full_project/
├─ Scylla.sln
├─ Scylla/                         原 Scylla UI + bridge client patch
│  ├─ XDbgBridge.h
│  └─ XDbgBridge.cpp
└─ ScyllaXBridgePlugin/             x64dbg plugin bridge server
   ├─ ScyllaXBridgePlugin.cpp
   ├─ plugin.h
   ├─ pluginmain.h
   └─ ScyllaXBridgePlugin.vcxproj
```

## 需要你手动放的东西

`ScyllaXBridgePlugin` 需要官方 x64dbg `pluginsdk` 文件夹。

放成这样：

```txt
ScyllaXBridgePlugin/
├─ pluginsdk/
│  ├─ bridgemain.h
│  ├─ _plugins.h
│  ├─ _scriptapi_module.h
│  ├─ _scriptapi_memory.h
│  ├─ x64dbg.lib
│  ├─ x64bridge.lib
│  ├─ x32dbg.lib
│  └─ x32bridge.lib
└─ ScyllaXBridgePlugin.cpp
```

x64dbg snapshot 或 `x64dbg-pluginsdk.zip` 里有这个目录。

## 编译

推荐 VS2022：

1. 打开 `Scylla.sln`。
2. 如果 VS 提示 Retarget，直接 retarget 到你机器上的 Windows SDK / v143。
3. 编译 `Release|x64`：
   - `Scylla` -> `ScyllaX.exe`
   - `ScyllaXBridgePlugin` -> `ScyllaXBridgePlugin.dp64`
4. 32 位同理编译 `Release|Win32`，输出 `.dp32`。

原 Scylla 工程比较老，Win32 配置里原本写了 `v90`。如果你没 VS2008，直接改成 `v143`。

## 部署

64 位：

```txt
x64dbg\release\x64\plugins\ScyllaXBridgePlugin.dp64
x64dbg\release\x64\plugins\ScyllaX.exe
```

32 位：

```txt
x64dbg\release\x32\plugins\ScyllaXBridgePlugin.dp32
x64dbg\release\x32\plugins\ScyllaX.exe
```

也可以把 `ScyllaX.exe` 放到 `x64dbg.exe/x32dbg.exe` 同级目录，插件会 fallback 找父目录。

## 使用

1. 用 x64dbg/x32dbg 打开目标。
2. 跑到你认为的 OEP/CIP 附近。
3. 菜单：`Plugins -> ScyllaXBridge -> Open ScyllaX UI`。
4. ScyllaX UI 会自动连接当前 debuggee，自动选中当前进程。
5. OEP 默认填当前 `cip`。
6. 后续 Dump / IAT Autosearch / Get Imports 走 Scylla UI，但底层内存读取走 x64dbg 插件。

## 现在这版的限制

这是 MVP，不是最终成品：

- 模块枚举走 `Script::Module::GetList`，正常情况下可解析 DLL 导出。
- 内存区域查询现在用 `DbgMemFindBaseAddr`，没有完整填充 Windows `MEMORY_BASIC_INFORMATION` 的 Protect/Type。
- Suspend/Resume/Terminate 在 bridge 模式下禁用，因为这应该交给 x64dbg 控制。
- 插件一次只服务一个 ScyllaX UI。
- 我没有在 Windows/VS 环境实际编译，只做了源码级整合；第一次编译可能需要按你机器的 x64dbg SDK 路径微调。

## 核心改动点

`ProcessAccessHelp.cpp`：

- `readMemoryFromProcess` -> bridge `XDBG_CMD_READ_MEMORY`
- `writeMemoryToProcess` -> bridge `XDBG_CMD_WRITE_MEMORY`
- `getProcessModules` -> bridge `XDBG_CMD_GET_MODULES`
- `getMemoryRegionFromAddress` -> bridge `XDBG_CMD_QUERY_MEMORY`
- `openProcessHandle` 在 bridge 模式下只设置 sentinel handle，不再打开目标进程

`ProcessLister.cpp`：

- bridge 模式下只返回一个 synthetic process：当前 x64dbg debuggee。

`MainGui.cpp`：

- bridge 模式启动后自动填充进程框并选择当前 debuggee。
- OEP 默认设置为 x64dbg 当前 `cip`。

`ScyllaXBridgePlugin.cpp`：

- x64dbg 菜单入口。
- 创建命名管道。
- 启动 `ScyllaX.exe --xdbg-pipe "..."`。
- 响应 ScyllaX 的内存读写、模块枚举、进程信息请求。
