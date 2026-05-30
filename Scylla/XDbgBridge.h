#pragma once

#include <windows.h>
#include <vector>
#include <string>

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

class XDbgBridge
{
public:
    static bool ConnectFromCommandLine(LPCTSTR commandLine);
    static bool Connect(const WCHAR* pipeName);
    static void Disconnect();
    static bool IsEnabled();

    static bool GetProcessInfo(XDbgProcessInfoWire& info);
    static bool GetModules(std::vector<XDbgModuleInfoWire>& modules);
    static bool ReadMemory(unsigned long long address, void* buffer, size_t size);
    static bool WriteMemory(unsigned long long address, const void* buffer, size_t size);
    static bool QueryMemory(unsigned long long address, XDbgMemoryRegionWire& region);
    static unsigned long long GetModuleSizeByBase(unsigned long long base);

private:
    static bool SendRequest(DWORD command, unsigned long long a, unsigned long long b, unsigned long long c, unsigned long long d, const void* payload, DWORD payloadSize, std::vector<BYTE>* response);
    static HANDLE pipeHandle;
    static bool enabled;
};
