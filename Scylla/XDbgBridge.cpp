#include "XDbgBridge.h"
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")

HANDLE XDbgBridge::pipeHandle = INVALID_HANDLE_VALUE;
bool XDbgBridge::enabled = false;

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

bool XDbgBridge::ConnectFromCommandLine(LPCTSTR commandLine)
{
    if(!commandLine || !*commandLine)
        return false;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(commandLine, &argc);
    if(!argv)
        return false;

    bool ok = false;
    for(int i = 0; i < argc; i++)
    {
        if((_wcsicmp(argv[i], L"--xdbg-pipe") == 0 || _wcsicmp(argv[i], L"/xdbg-pipe") == 0) && i + 1 < argc)
        {
            ok = Connect(argv[i + 1]);
            break;
        }
    }

    LocalFree(argv);
    return ok;
}

bool XDbgBridge::Connect(const WCHAR* pipeName)
{
    Disconnect();

    if(!pipeName || !*pipeName)
        return false;

    pipeHandle = CreateFileW(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(pipeHandle == INVALID_HANDLE_VALUE)
        return false;

    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(pipeHandle, &mode, NULL, NULL);

    std::vector<BYTE> response;
    if(!SendRequest(XDBG_CMD_HELLO, 0, 0, 0, 0, NULL, 0, &response))
    {
        Disconnect();
        return false;
    }

    enabled = true;
    return true;
}

void XDbgBridge::Disconnect()
{
    if(pipeHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(pipeHandle);
        pipeHandle = INVALID_HANDLE_VALUE;
    }
    enabled = false;
}

bool XDbgBridge::IsEnabled()
{
    return enabled && pipeHandle != INVALID_HANDLE_VALUE;
}

bool XDbgBridge::SendRequest(DWORD command, unsigned long long a, unsigned long long b, unsigned long long c, unsigned long long d, const void* payload, DWORD payloadSize, std::vector<BYTE>* response)
{
    if(pipeHandle == INVALID_HANDLE_VALUE)
        return false;

    XDbgPacket req = {0};
    req.magic = SCYLLAX_PIPE_MAGIC;
    req.version = SCYLLAX_PIPE_VERSION;
    req.command = command;
    req.a = a;
    req.b = b;
    req.c = c;
    req.d = d;
    req.size = payloadSize;

    if(!writeExact(pipeHandle, &req, sizeof(req)))
        return false;

    if(payloadSize && payload)
    {
        if(!writeExact(pipeHandle, payload, payloadSize))
            return false;
    }

    XDbgPacket reply = {0};
    if(!readExact(pipeHandle, &reply, sizeof(reply)))
        return false;

    if(reply.magic != SCYLLAX_PIPE_MAGIC || reply.version != SCYLLAX_PIPE_VERSION || reply.command != command || reply.status == 0)
        return false;

    if(response)
    {
        response->clear();
        if(reply.size)
        {
            response->resize(reply.size);
            if(!readExact(pipeHandle, &(*response)[0], reply.size))
                return false;
        }
    }
    else if(reply.size)
    {
        std::vector<BYTE> dummy(reply.size);
        if(!readExact(pipeHandle, &dummy[0], reply.size))
            return false;
    }

    return true;
}

bool XDbgBridge::GetProcessInfo(XDbgProcessInfoWire& info)
{
    ZeroMemory(&info, sizeof(info));
    std::vector<BYTE> response;
    if(!SendRequest(XDBG_CMD_GET_PROCESS, 0, 0, 0, 0, NULL, 0, &response))
        return false;
    if(response.size() < sizeof(XDbgProcessInfoWire))
        return false;
    memcpy(&info, &response[0], sizeof(XDbgProcessInfoWire));
    return true;
}

bool XDbgBridge::GetModules(std::vector<XDbgModuleInfoWire>& modules)
{
    modules.clear();
    std::vector<BYTE> response;
    if(!SendRequest(XDBG_CMD_GET_MODULES, 0, 0, 0, 0, NULL, 0, &response))
        return false;
    if(response.empty())
        return true;
    if(response.size() % sizeof(XDbgModuleInfoWire) != 0)
        return false;

    size_t count = response.size() / sizeof(XDbgModuleInfoWire);
    modules.resize(count);
    memcpy(&modules[0], &response[0], response.size());
    return true;
}

bool XDbgBridge::ReadMemory(unsigned long long address, void* buffer, size_t size)
{
    if(!buffer || size == 0)
        return false;

    const DWORD MAX_CHUNK = 0x100000;
    BYTE* out = (BYTE*)buffer;
    size_t done = 0;

    while(done < size)
    {
        DWORD chunk = (DWORD)((size - done) > MAX_CHUNK ? MAX_CHUNK : (size - done));
        std::vector<BYTE> response;
        if(!SendRequest(XDBG_CMD_READ_MEMORY, address + done, chunk, 0, 0, NULL, 0, &response))
            return false;
        if(response.size() != chunk)
            return false;
        memcpy(out + done, &response[0], chunk);
        done += chunk;
    }
    return true;
}

bool XDbgBridge::WriteMemory(unsigned long long address, const void* buffer, size_t size)
{
    if(!buffer || size == 0)
        return false;

    const DWORD MAX_CHUNK = 0x100000;
    const BYTE* in = (const BYTE*)buffer;
    size_t done = 0;

    while(done < size)
    {
        DWORD chunk = (DWORD)((size - done) > MAX_CHUNK ? MAX_CHUNK : (size - done));
        if(!SendRequest(XDBG_CMD_WRITE_MEMORY, address + done, chunk, 0, 0, in + done, chunk, NULL))
            return false;
        done += chunk;
    }
    return true;
}

bool XDbgBridge::QueryMemory(unsigned long long address, XDbgMemoryRegionWire& region)
{
    ZeroMemory(&region, sizeof(region));
    std::vector<BYTE> response;
    if(!SendRequest(XDBG_CMD_QUERY_MEMORY, address, 0, 0, 0, NULL, 0, &response))
        return false;
    if(response.size() < sizeof(XDbgMemoryRegionWire))
        return false;
    memcpy(&region, &response[0], sizeof(XDbgMemoryRegionWire));
    return region.base != 0 && region.size != 0;
}

unsigned long long XDbgBridge::GetModuleSizeByBase(unsigned long long base)
{
    std::vector<XDbgModuleInfoWire> modules;
    if(!GetModules(modules))
        return 0;

    for(size_t i = 0; i < modules.size(); i++)
    {
        if(modules[i].base == base)
            return modules[i].size;
    }
    return 0;
}
