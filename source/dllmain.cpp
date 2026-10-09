#include "StdInc.h"
#include "platform/win/WinPlatform.h"
#include <cstdlib>
#include <app_debug.h>
#include "extensions/CommandLine.h"
#include "extensions/debug.hpp"
#include "extensions/Configuration.hpp"
#include "InjectHooksMain.h"
#include <dbghelp.h>
#include <filesystem>
#include <format>
#include <chrono>

static constexpr auto DEFAULT_INI_FILENAME = "gta-reversed.ini";

#include "extensions/Configs/FastLoader.hpp"
#include "extensions/Configs/Miscellaneous.hpp"
#include "dllmain.h"

HMODULE s_HandleOfDLL{};

void LoadConfigurations() {
    // Firstly load the INI into the memory.
    g_ConfigurationMgr.Load(DEFAULT_INI_FILENAME);

    // Then load all specific configurations.
    g_FastLoaderConfig.Load();
    g_MiscConfig.Load();
    // ...
}


// ExitProcess IAT hook (original exe slot 0x85820C): logs the stack of whoever calls exit().

// ---- File-Win32 failure capture (EXE IAT): log ONLY failures with GetLastError ----
// Goal: catch the ERROR_INVALID_HANDLE(6) that becomes the game's exit code.
static decltype(&CreateFileA)  oCreateFileA;
static decltype(&CreateFileW)  oCreateFileW;
static decltype(&ReadFile)     oReadFile;
static decltype(&SetFilePointer) oSetFilePointer;
static decltype(&CloseHandle)  oCloseHandle;
static decltype(&GetOverlappedResult) oGetOverlappedResult;

static HANDLE WINAPI H_CreateFileA(LPCSTR a0, DWORD a1, DWORD a2, LPSECURITY_ATTRIBUTES a3, DWORD a4, DWORD a5, HANDLE a6) {
    auto r = oCreateFileA(a0, a1, a2, a3, a4, a5, a6);
    if (r == INVALID_HANDLE_VALUE)
        NOTSA_LOG_ERR("!!! WIN32 FAIL CreateFileA({}) -> GLE={}", a0 ? a0 : "?", GetLastError());
    return r;
}
static HANDLE WINAPI H_CreateFileW(LPCWSTR a0, DWORD a1, DWORD a2, LPSECURITY_ATTRIBUTES a3, DWORD a4, DWORD a5, HANDLE a6) {
    auto r = oCreateFileW(a0, a1, a2, a3, a4, a5, a6);
    if (r == INVALID_HANDLE_VALUE)
        NOTSA_LOG_ERR("!!! WIN32 FAIL CreateFileW -> GLE={}", GetLastError());
    return r;
}
static BOOL WINAPI H_ReadFile(HANDLE h, LPVOID b, DWORD n, LPDWORD got, LPOVERLAPPED ov) {
    auto r = oReadFile(h, b, n, got, ov);
    if (!r && GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_BROKEN_PIPE)
        NOTSA_LOG_ERR("!!! WIN32 FAIL ReadFile(h={:#x}, n={}) -> GLE={}", (uintptr_t)h, n, GetLastError());
    return r;
}
static DWORD WINAPI H_SetFilePointer(HANDLE h, LONG dist, PLONG base, DWORD method) {
    auto r = oSetFilePointer(h, dist, base, method);
    if (r == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        NOTSA_LOG_ERR("!!! WIN32 FAIL SetFilePointer(h={:#x}, dist={}, method={}) -> GLE={}", (uintptr_t)h, dist, method, GetLastError());
    return r;
}
static BOOL WINAPI H_CloseHandle(HANDLE h) {
    auto r = oCloseHandle(h);
    if (!r)
        NOTSA_LOG_ERR("!!! WIN32 FAIL CloseHandle(h={:#x}) -> GLE={}", (uintptr_t)h, GetLastError());
    return r;
}
static BOOL WINAPI H_GetOverlappedResult(HANDLE h, LPOVERLAPPED ov, LPDWORD got, BOOL wait) {
    auto r = oGetOverlappedResult(h, ov, got, wait);
    if (!r)
        NOTSA_LOG_ERR("!!! WIN32 FAIL GetOverlappedResult(h={:#x}) -> GLE={}", (uintptr_t)h, GetLastError());
    return r;
}
static void InstallWin32FailHooks() {
    auto* base = (uint8_t*)0x400000;
    auto* dos = (IMAGE_DOS_HEADER*)base;
    auto* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    auto* imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    int n = 0;
    while (imp->Name) {
        auto* thunk = (IMAGE_THUNK_DATA*)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto* iat = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        for (int i = 0; thunk[i].u1.AddressOfData; i++) {
            if (thunk[i].u1.AddressOfData & 0x80000000) continue;
            auto* ibn = (IMAGE_IMPORT_BY_NAME*)(base + thunk[i].u1.AddressOfData);
            const char* fn = (const char*)ibn->Name;
            void** slot = (void**)&iat[i].u1.Function;
            void* hook = nullptr; void** orig = nullptr;
            if (!strcmp(fn, "CreateFileA")) { hook = (void*)&H_CreateFileA; orig = (void**)&oCreateFileA; }
            else if (!strcmp(fn, "CreateFileW")) { hook = (void*)&H_CreateFileW; orig = (void**)&oCreateFileW; }
            else if (!strcmp(fn, "ReadFile")) { hook = (void*)&H_ReadFile; orig = (void**)&oReadFile; }
            else if (!strcmp(fn, "SetFilePointer")) { hook = (void*)&H_SetFilePointer; orig = (void**)&oSetFilePointer; }
            else if (!strcmp(fn, "CloseHandle")) { hook = (void*)&H_CloseHandle; orig = (void**)&oCloseHandle; }
            else if (!strcmp(fn, "GetOverlappedResult")) { hook = (void*)&H_GetOverlappedResult; orig = (void**)&oGetOverlappedResult; }
            if (hook) {
                DWORD old{};
                if (VirtualProtect(slot, 4, PAGE_READWRITE, &old)) {
                    *orig = *slot;
                    *slot = hook;
                    VirtualProtect(slot, 4, old, &old);
                    n++;
                }
            }
        }
        imp++;
    }
    NOTSA_LOG_INFO("File-Win32 fail hooks installed: {}", n);
}

static void(WINAPI* g_realExitProcess)(UINT) = nullptr;
static void WINAPI LoggedExitProcess(UINT code) {
    NOTSA_LOG_ERR("!!! ExitProcess({}) CALLED - origin stack follows", code);
    {   // capture a full-memory minidump BEFORE exiting (exit() without exception leaves no VEH dump)
        std::error_code ec{};
        fs::create_directory("dumps", ec);
        HANDLE hFile = CreateFileA(
            std::format("dumps/dump_exit_{:%Y_%m_%d_%H_%M_%S}.dmp", std::chrono::utc_clock::now()).c_str(),
            GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                (MINIDUMP_TYPE)(MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData),
                NULL, NULL, NULL);
            CloseHandle(hFile);
        }
    }
    CONTEXT ctx{};
    RtlCaptureContext(&ctx);
    const auto* sp = (const uint32_t*)ctx.Esp;
    const auto asiBase = (uintptr_t)s_HandleOfDLL;
    // symbolized walk: dbghelp is initialized by notsa::debug::LoadSymbols
    {
        HANDLE hProc = GetCurrentProcess();
        for (int i = 0; i < 4096; i++) {
            const auto v = sp[i];
            if (v == 0) continue;
            bool asiHit = asiBase <= v && v < asiBase + 0x1900000;
            bool exeHit = 0x401000 <= v && v < 0x857000;
            if (!asiHit && !exeHit) continue;
            char symBuf[sizeof(SYMBOL_INFO) + 256] = {0};
            auto* si = (SYMBOL_INFO*)symBuf;
            si->SizeOfStruct = sizeof(SYMBOL_INFO);
            si->MaxNameLen = 255;
            DWORD64 disp = 0;
            if (SymFromAddr(hProc, (DWORD64)v, &disp, si)) {
                NOTSA_LOG_INFO("  [esp+0x{:04X}] {} RVA 0x{:08X}  {}+{}",
                    i*4, asiHit ? "ASI" : "EXE", asiHit ? (v - asiBase) : (v - 0x400000),
                    si->Name, (uint32_t)disp);
            } else {
                NOTSA_LOG_INFO("  [esp+0x{:04X}] {} RVA 0x{:08X}",
                    i*4, asiHit ? "ASI" : "EXE", asiHit ? (v - asiBase) : (v - 0x400000));
            }
        }
    }
    g_realExitProcess(code);
}
static void InstallExitProcessIatHook() {
    const auto slot = (void**)0x85820C; // ExitProcess IAT entry of gta_sa compact exe (verified by disasm)
    DWORD old{};
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old))
        return;
    g_realExitProcess = (void(WINAPI*)(UINT))*slot;
    *slot = (void*)&LoggedExitProcess;
    VirtualProtect(slot, sizeof(void*), old, &old);
    NOTSA_LOG_INFO("ExitProcess IAT hook installed (orig 0x{:08X})", (uintptr_t)g_realExitProcess);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
        InstallExitProcessIatHook();
        // Heap corruption detection: die AT THE MOMENT of corruption (STATUS_HEAP_CORRUPTION
        // 0xC0000374, caught by our VEH with the corruptor's stack) instead of minutes later.
        HeapSetInformation(GetProcessHeap(), HeapEnableTerminationOnCorruption, nullptr, 0);
        s_HandleOfDLL = hModule;

        // Fail if RenderWare has already been started
        if (*(RwCamera**)0xC1703C) {
            MessageBox(NULL, "gta_reversed failed to load (RenderWare has already been started)", "Error", MB_ICONERROR | MB_OK);
            return FALSE;
        }

        std::setlocale(LC_ALL, "en_US.UTF-8");

        notsa::debug::DisplayConsole();
        notsa::debug::LoadSymbols(); // Used by logging
        notsa::Logging::CreateInstance();

        CommandLine::Load(__argc, __argv);
        if (CommandLine::s_WaitForDebugger) {
            notsa::debug::WaitForDebugger();
        }
        LoadConfigurations();

        // CRT invalid-parameter capture: Release CRT kills the process (ExitProcess with
        // a status code, no exception, no VEH dump) on invalid FILE*/handle args.
        _set_invalid_parameter_handler([](const wchar_t* expr, const wchar_t* fn, const wchar_t* file, unsigned line, uintptr_t ctx) {
            char e[128]{}, f[128]{}, fl[256]{};
            WideCharToMultiByte(CP_UTF8, 0, expr ? expr : L"?", -1, e, 127, nullptr, nullptr);
            WideCharToMultiByte(CP_UTF8, 0, fn ? fn : L"?", -1, f, 127, nullptr, nullptr);
            WideCharToMultiByte(CP_UTF8, 0, file ? file : L"?", -1, fl, 255, nullptr, nullptr);
            NOTSA_LOG_ERR("!!! CRT INVALID PARAMETER: {} in {} ({}:{})", e, f, fl, line);
            return;
        });
        // Detour CRT exit() (0x823BDB, static CRT of the compact exe) to capture the
        // CALLER's stack at the moment of exit (before CRT unwinds it).
        {
            constexpr uintptr_t CRT_EXIT = 0x823BDB;
            auto* src = (uint8_t*)CRT_EXIT;
            DWORD old{};
            if (VirtualProtect(src, 8, PAGE_EXECUTE_READWRITE, &old)) {
                static uint8_t orig[8];
                memcpy(orig, src, 8);
                // trampoline: log caller, run stolen bytes (push ebp; mov ebp,esp; push esi; push edi),
                // then jump back to CRT_EXIT+5 (push 8; call __lock ... continue)
                auto handler = +[]() {
                    CONTEXT ctx{};
                    RtlCaptureContext(&ctx);
                    const auto* sp = (const uint32_t*)ctx.Esp;
                    const auto asiBase = (uintptr_t)s_HandleOfDLL;
                    NOTSA_LOG_ERR("!!! CRT exit({:#x}) CALLED - caller stack (first 192 slots):",
                        (uint32_t)(ctx.Ebp ? *(const uint32_t*)((const uint8_t*)ctx.Ebp + 8) : 0));
                    HANDLE hProc = GetCurrentProcess();
                    for (int i = 0; i < 192; i++) {
                        const auto v = sp[i];
                        if (v == 0) continue;
                        bool asiHit = asiBase <= v && v < asiBase + 0x1900000;
                        bool exeHit = 0x401000 <= v && v < 0x857000;
                        if (!asiHit && !exeHit) continue;
                        char symBuf[sizeof(SYMBOL_INFO) + 256] = {0};
                        auto* si = (SYMBOL_INFO*)symBuf;
                        si->SizeOfStruct = sizeof(SYMBOL_INFO);
                        si->MaxNameLen = 255;
                        DWORD64 disp = 0;
                        const char* name = "";
                        if (SymFromAddr(hProc, (DWORD64)v, &disp, si)) name = si->Name;
                        NOTSA_LOG_ERR("  [esp+0x{:03X}] {} RVA 0x{:08X} {}+{}", i*4,
                            asiHit ? "ASI" : "EXE", asiHit ? (v - asiBase) : (v - 0x400000), name, (uint32_t)disp);
                    }
                };
                static void (*s_handler)() = handler;
                // place: push ebp; mov ebp,esp; push esi; push edi  (6 bytes) -> replace with
                // call rel32 our thunk (5 bytes) + 1 NOP; stolen bytes run inside thunk.
                static uint8_t* backAddr = nullptr;
                #pragma pack(push,1)
                struct Thunk {
                    uint8_t push_ebp;   // stolen
                    uint8_t mov_ebp_esp[2];
                    uint8_t push_esi;
                    uint8_t push_edi;
                    uint8_t call_handler[5];  // E8 rel32 -> handler
                    uint8_t jmp_back[5];      // E9 rel32 -> CRT_EXIT+6
                };
                #pragma pack(pop)
                static Thunk thunk;
                auto makeRel = [](void* from, void* to, size_t insnLen) {
                    return (int32_t)((uintptr_t)to - ((uintptr_t)from + insnLen));
                };
                thunk = Thunk{
                    0x55, {0x8B, 0xEC}, 0x56, 0x57,
                    {0xE8, 0,0,0,0},
                    {0xE9, 0,0,0,0},
                };
                *(int32_t*)(thunk.call_handler + 1) = makeRel(&thunk.call_handler, (void*)handler, 5);
                *(int32_t*)(thunk.jmp_back + 1) = makeRel(&thunk.jmp_back, (void*)(CRT_EXIT + 5), 5);
                // patch site: E9 rel32 -> thunk; +3 NOPs (total 6 bytes replaced)
                src[0] = 0xE9;
                *(int32_t*)(src + 1) = makeRel(src, &thunk, 5);
                src[5] = 0x90; src[6] = 0x90; src[7] = 0x90;
                VirtualProtect(src, 8, old, &old);
                FlushInstructionCache(GetCurrentProcess(), src, 8);
                NOTSA_LOG_INFO("CRT exit() detour installed");
            }
        }
        InstallWin32FailHooks();
        ReversibleHooks::RHManager::CreateInstance();
        InjectHooksMain();

        break;
    }
    case DLL_PROCESS_DETACH: {
        if (lpReserved == nullptr) {
            NOTSA_LOG_INFO("DLL_PROCESS_DETACH: Shutting down normally...");

            ReversibleHooks::RHManager::DestroyInstance();
            notsa::Logging::DestroyInstance();
            notsa::debug::UnloadSymbols();
        } else { // This is pretty much the only thing that's ever reached
            NOTSA_LOG_INFO("DLL_PROCESS_DETACH: Process is terminating, shutting down only what's necessary");
            { // EXIT-ORIGIN TRACE: raw stack scan (frame-pointer walk dies in CRT frames)
                CONTEXT ctx{};
                RtlCaptureContext(&ctx);
                const auto* sp = (const uint32_t*)ctx.Esp;
                HMODULE hAsi{};
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&DllMain, &hAsi);
                const auto asiBase = (uintptr_t)hAsi;
                NOTSA_LOG_INFO("=== EXIT-ORIGIN RAW STACK SCAN (asi base {:X}) ===", asiBase);
                for (int i = 0; i < 4096; i++) {
                    const auto v = sp[i];
                    if (v == 0) continue;
                    if (asiBase <= v && v < asiBase + 0x1900000)
                        NOTSA_LOG_INFO("  [esp+0x{:04X}] ASI RVA 0x{:08X}", i*4, v - asiBase);
                    else if (0x401000 <= v && v < 0x857000)
                        NOTSA_LOG_INFO("  [esp+0x{:04X}] EXE RVA 0x{:08X}", i*4, v - 0x400000);
                }
            }

            notsa::Logging::DestroyInstance();
        }
        break;
    }
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

HMODULE notsa::GetDLLHandle() {
    return s_HandleOfDLL;
}
