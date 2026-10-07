#include "StdInc.h"
#include "platform/win/WinPlatform.h"
#include <cstdlib>
#include <app_debug.h>
#include "extensions/CommandLine.h"
#include "extensions/debug.hpp"
#include "extensions/Configuration.hpp"
#include "InjectHooksMain.h"

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
static void(WINAPI* g_realExitProcess)(UINT) = nullptr;
static void WINAPI LoggedExitProcess(UINT code) {
    NOTSA_LOG_ERR("!!! ExitProcess({}) CALLED - origin stack follows", code);
    CONTEXT ctx{};
    RtlCaptureContext(&ctx);
    const auto* sp = (const uint32_t*)ctx.Esp;
    const auto asiBase = (uintptr_t)s_HandleOfDLL;
    for (int i = 0; i < 2048; i++) {
        const auto v = sp[i];
        if (v == 0) continue;
        if (asiBase <= v && v < asiBase + 0x1900000)
            NOTSA_LOG_INFO("  [esp+0x{:04X}] ASI RVA 0x{:08X}", i*4, v - asiBase);
        else if (0x401000 <= v && v < 0x857000)
            NOTSA_LOG_INFO("  [esp+0x{:04X}] EXE RVA 0x{:08X}", i*4, v - 0x400000);
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
