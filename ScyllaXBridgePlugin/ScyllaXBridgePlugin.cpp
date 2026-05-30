#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>

#include "plugin.h"

#define PLUGIN_NAME "ScyllaXBridge"
#define PLUGIN_VERSION 1
#define MENU_OPEN_SCYLLAX 1

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

enum XDbgCommand
{
    XDBG_CMD_HELLO = 1,
    XDBG_CMD_GET_PROCESS = 2,
    XDBG_CMD_GET_MODULES = 3,
    XDBG_CMD_READ_MEMORY = 4,
    XDBG_CMD_WRITE_MEMORY = 5,
    XDBG_CMD_QUERY_MEMORY = 6
};

struct XDbgProcessInfoWire
{
    DWORD pid;
    DWORD sessionId;
    DWORD reserved;
    unsigned long long imageBase;
    unsigned long long imageSize;
    unsigned long long entryPointVa;
    unsigned long long currentIp;
    WCHAR filename[MAX_PATH];
    WCHAR fullPath[MAX_PATH];
};

struct XDbgModuleInfoWire
{
    unsigned long long base;
    unsigned long long size;
    unsigned long long entry;
    WCHAR filename[MAX_PATH];
    WCHAR fullPath[MAX_PATH];
};

struct XDbgMemoryRegionWire
{
    unsigned long long base;
    unsigned long long size;
    DWORD protect;
    DWORD state;
    DWORD type;
};

int g_pluginHandle = 0;
HWND g_hwndDlg = 0;
HMODULE g_module = 0;

static bool readExact(HANDLE h, void* buffer, DWORD size)
{
    BYTE* p = (BYTE*)buffer;
    DWORD done = 0;
    while(done < size)
    {
        DWORD got = 0;
        if(!ReadFile(h, p + done, size - done, &got, NULL) || got == 0)
            return false;
        done += got;
    }
    return true;
}

static bool writeExact(HANDLE h, const void* buffer, DWORD size)
{
    const BYTE* p = (const BYTE*)buffer;
    DWORD done = 0;
    while(done < size)
    {
        DWORD wrote = 0;
        if(!WriteFile(h, p + done, size - done, &wrote, NULL) || wrote == 0)
            return false;
        done += wrote;
    }
    return true;
}

static bool sendReply(HANDLE pipe, DWORD command, bool ok, const void* payload = nullptr, DWORD payloadSize = 0)
{
    XDbgPacket reply = {0};
    reply.magic = SCYLLAX_PIPE_MAGIC;
    reply.version = SCYLLAX_PIPE_VERSION;
    reply.command = command;
    reply.status = ok ? 1 : 0;
    reply.size = ok ? payloadSize : 0;

    if(!writeExact(pipe, &reply, sizeof(reply)))
        return false;
    if(ok && payloadSize && payload)
        return writeExact(pipe, payload, payloadSize);
    return true;
}

static void ansiToWide(const char* s, WCHAR* out, size_t outCount)
{
    if(!out || outCount == 0)
        return;
    out[0] = 0;
    if(!s || !*s)
        return;

    int r = MultiByteToWideChar(CP_UTF8, 0, s, -1, out, (int)outCount);
    if(r <= 0)
        MultiByteToWideChar(CP_ACP, 0, s, -1, out, (int)outCount);
    out[outCount - 1] = 0;
}

static const WCHAR* fileNameOnly(const WCHAR* path)
{
    if(!path)
        return L"";
    const WCHAR* slash1 = wcsrchr(path, L'\\');
    const WCHAR* slash2 = wcsrchr(path, L'/');
    const WCHAR* slash = slash1 > slash2 ? slash1 : slash2;
    return slash ? slash + 1 : path;
}

static bool buildProcessInfo(XDbgProcessInfoWire& info)
{
    ZeroMemory(&info, sizeof(info));

    Script::Module::ModuleInfo mainModule = {0};
    if(!Script::Module::GetMainModuleInfo(&mainModule))
        return false;

    info.pid = DbgGetProcessId();
    info.imageBase = (unsigned long long)mainModule.base;
    info.imageSize = (unsigned long long)mainModule.size;
    info.entryPointVa = (unsigned long long)mainModule.entry;
    info.currentIp = (unsigned long long)DbgValFromString("cip");

    WCHAR path[MAX_PATH] = {0};
    ansiToWide(mainModule.path, path, _countof(path));
    wcscpy_s(info.fullPath, path[0] ? path : L"xdbg-debuggee.exe");
    wcscpy_s(info.filename, fileNameOnly(info.fullPath));
    return true;
}

static bool buildModuleList(std::vector<XDbgModuleInfoWire>& out)
{
    out.clear();

    BridgeList modules;
    if(!Script::Module::GetList(&modules))
        return false;

    out.reserve(modules.Count());
    for(int i = 0; i < modules.Count(); i++)
    {
        auto& mod = modules[i];
        XDbgModuleInfoWire item = {0};
        item.base = (unsigned long long)mod.base;
        item.size = (unsigned long long)mod.size;
        item.entry = (unsigned long long)mod.entry;
        ansiToWide(mod.path, item.fullPath, _countof(item.fullPath));
        if(!item.fullPath[0])
            ansiToWide(mod.name, item.fullPath, _countof(item.fullPath));
        wcscpy_s(item.filename, fileNameOnly(item.fullPath));
        out.push_back(item);
    }

    return true;
}

static bool queryMemory(unsigned long long addr, XDbgMemoryRegionWire& region)
{
    ZeroMemory(&region, sizeof(region));

    duint regionSize = 0;
    duint base = DbgMemFindBaseAddr((duint)addr, &regionSize);
    if(!base || !regionSize)
        return false;

    region.base = (unsigned long long)base;
    region.size = (unsigned long long)regionSize;
    region.protect = 0;
    region.state = MEM_COMMIT;
    region.type = 0;
    return true;
}

static void servePipe(HANDLE pipe)
{
    while(true)
    {
        XDbgPacket req = {0};
        if(!readExact(pipe, &req, sizeof(req)))
            break;

        if(req.magic != SCYLLAX_PIPE_MAGIC || req.version != SCYLLAX_PIPE_VERSION)
            break;

        std::vector<BYTE> payload;
        if(req.size)
        {
            payload.resize(req.size);
            if(!readExact(pipe, &payload[0], req.size))
                break;
        }

        switch(req.command)
        {
        case XDBG_CMD_HELLO:
            sendReply(pipe, req.command, true);
            break;

        case XDBG_CMD_GET_PROCESS:
        {
            XDbgProcessInfoWire info;
            bool ok = buildProcessInfo(info);
            sendReply(pipe, req.command, ok, &info, sizeof(info));
            break;
        }

        case XDBG_CMD_GET_MODULES:
        {
            std::vector<XDbgModuleInfoWire> modules;
            bool ok = buildModuleList(modules);
            sendReply(pipe, req.command, ok, modules.empty() ? nullptr : &modules[0], (DWORD)(modules.size() * sizeof(XDbgModuleInfoWire)));
            break;
        }

        case XDBG_CMD_READ_MEMORY:
        {
            DWORD size = (DWORD)req.b;
            std::vector<BYTE> data(size);
            duint read = 0;
            bool ok = size != 0 && Script::Memory::Read((duint)req.a, &data[0], size, &read) && read == size;
            sendReply(pipe, req.command, ok, ok ? &data[0] : nullptr, ok ? size : 0);
            break;
        }

        case XDBG_CMD_WRITE_MEMORY:
        {
            duint written = 0;
            bool ok = req.size != 0 && Script::Memory::Write((duint)req.a, &payload[0], req.size, &written) && written == req.size;
            sendReply(pipe, req.command, ok);
            break;
        }

        case XDBG_CMD_QUERY_MEMORY:
        {
            XDbgMemoryRegionWire region;
            bool ok = queryMemory(req.a, region);
            sendReply(pipe, req.command, ok, &region, sizeof(region));
            break;
        }

        default:
            sendReply(pipe, req.command, false);
            break;
        }
    }
}

static bool getPluginDirectory(WCHAR* dir, size_t dirCount)
{
    if(!GetModuleFileNameW(g_module, dir, (DWORD)dirCount))
        return false;
    WCHAR* slash = wcsrchr(dir, L'\\');
    if(!slash)
        return false;
    *slash = 0;
    return true;
}

static bool launchScyllaX(const WCHAR* pipeName)
{
    WCHAR dir[MAX_PATH] = {0};
    if(!getPluginDirectory(dir, _countof(dir)))
        return false;

    WCHAR exe[MAX_PATH] = {0};
    swprintf_s(exe, L"%s\\ScyllaX.exe", dir);

    DWORD attr = GetFileAttributesW(exe);
    if(attr == INVALID_FILE_ATTRIBUTES)
    {
        // Fallback: plugin may be in plugins\\, ScyllaX.exe next to x32dbg/x64dbg.
        WCHAR parent[MAX_PATH] = {0};
        wcscpy_s(parent, dir);
        WCHAR* slash = wcsrchr(parent, L'\\');
        if(slash)
        {
            *slash = 0;
            swprintf_s(exe, L"%s\\ScyllaX.exe", parent);
        }
    }

    WCHAR cmdLine[MAX_PATH * 3] = {0};
    swprintf_s(cmdLine, L"\"%s\" --xdbg-pipe \"%s\"", exe, pipeName);

    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    BOOL ok = CreateProcessW(exe, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if(ok)
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return ok != FALSE;
}

static DWORD WINAPI bridgeThread(LPVOID)
{
    if(!DbgIsDebugging())
    {
        _plugin_logputs("[ScyllaXBridge] No active debuggee.");
        return 0;
    }

    WCHAR pipeName[128] = {0};
    swprintf_s(pipeName, L"\\\\.\\pipe\\ScyllaXBridge_%lu_%lu", GetCurrentProcessId(), GetTickCount());

    HANDLE pipe = CreateNamedPipeW(
        pipeName,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        0x100000,
        0x100000,
        0,
        NULL);

    if(pipe == INVALID_HANDLE_VALUE)
    {
        _plugin_logprintf("[ScyllaXBridge] CreateNamedPipe failed: %u\n", GetLastError());
        return 0;
    }

    if(!launchScyllaX(pipeName))
    {
        _plugin_logputs("[ScyllaXBridge] Failed to launch ScyllaX.exe. Put it next to the plugin or next to x64dbg.exe/x32dbg.exe.");
        CloseHandle(pipe);
        return 0;
    }

    BOOL connected = ConnectNamedPipe(pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    if(connected)
    {
        _plugin_logputs("[ScyllaXBridge] ScyllaX connected.");
        servePipe(pipe);
    }

    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    _plugin_logputs("[ScyllaXBridge] ScyllaX disconnected.");
    return 0;
}

extern "C" __declspec(dllexport) bool pluginit(PLUG_INITSTRUCT* initStruct)
{
    initStruct->pluginVersion = PLUGIN_VERSION;
    initStruct->sdkVersion = PLUG_SDKVERSION;
    strncpy_s(initStruct->pluginName, PLUGIN_NAME, _TRUNCATE);
    g_pluginHandle = initStruct->pluginHandle;
    _plugin_logputs("[ScyllaXBridge] loaded");
    return true;
}

extern "C" __declspec(dllexport) bool plugstop()
{
    _plugin_logputs("[ScyllaXBridge] unloaded");
    return true;
}

extern "C" __declspec(dllexport) void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
    g_hwndDlg = setupStruct->hwndDlg;
    _plugin_menuaddentry(setupStruct->hMenu, MENU_OPEN_SCYLLAX, "Open ScyllaX UI");
}

extern "C" __declspec(dllexport) void CBMENUENTRY(CBTYPE cbType, void* callbackInfo)
{
    PLUG_CB_MENUENTRY* info = (PLUG_CB_MENUENTRY*)callbackInfo;
    if(info && info->hEntry == MENU_OPEN_SCYLLAX)
        CreateThread(NULL, 0, bridgeThread, NULL, 0, NULL);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    if(fdwReason == DLL_PROCESS_ATTACH)
        g_module = hinstDLL;
    return TRUE;
}
