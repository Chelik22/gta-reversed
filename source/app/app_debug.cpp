#include "StdInc.h"

#include "app_debug.h"
#include "platform/win/WinPlatform.h"
#include <windows.h>
#include <DbgHelp.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <extensions/debug.hpp>

#define FINAL 0

#ifndef FINAL
RtCharset* debugCharset;
bool g_bDebugRenderGroups;
#endif

#if !defined(FINAL)
static bool charsetOpen;
void OpenCharsetSafe() {
    if (!charsetOpen)
        RtCharsetOpen();
    charsetOpen = true;
}
#endif

// 0x734610
void CreateDebugFont() {
#ifndef FINAL
    RwRGBA color = { 255, 255, 128, 255 };
    RwRGBA colorbg = { 0, 0, 0, 0 };
    OpenCharsetSafe();
    debugCharset = RtCharsetCreate(&color, &colorbg);
#endif
}

// 0x734620
void DestroyDebugFont() {
#ifndef FINAL
    RtCharsetDestroy(debugCharset);
    RtCharsetClose();
    charsetOpen = false;
#endif
}

// unused
// 0x734630
void ObrsPrintfString(const char* str, int16 x, int16 y) {
#ifndef FINAL
    RtCharsetPrintBuffered(debugCharset, str, x * 8, y * 16, true);
#endif
}

// 0x734640
void FlushObrsPrintfs() {
#ifndef FINAL
    RtCharsetBufferFlush();
#endif
}

// This probably should be in winps :D
LONG WINAPI WindowsExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    // If this function itself crashes it's invoked again
    // So let's prevent the recusion with this simple hack
    static bool s_HasHandled = false;
    if (s_HasHandled) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    s_HasHandled = true;

    spdlog::apply_all([](auto&& logger) {
        logger->dump_backtrace();
    });
    
    const auto Section = [](const char* name) {
        SPDLOG_INFO("*********{}**********", name);
    };

    Section("UNHANDLED EXCEPTION");

    SPDLOG_INFO("Exception Code: {:#010x}", pExceptionInfo->ExceptionRecord->ExceptionCode);
    SPDLOG_INFO("Exception Flags: {:#010x}", pExceptionInfo->ExceptionRecord->ExceptionFlags);
    SPDLOG_INFO("Exception Address: {:#010x}", (uintptr_t)pExceptionInfo->ExceptionRecord->ExceptionAddress);

    // Dump exception parameters
    Section("PARAMETERS");
    {
        SPDLOG_INFO("Parameters[{}]:", pExceptionInfo->ExceptionRecord->NumberParameters);
        for (DWORD i = 0; i < pExceptionInfo->ExceptionRecord->NumberParameters; i++) {
            SPDLOG_INFO("{:>8}: {:#010x}", i, pExceptionInfo->ExceptionRecord->ExceptionInformation[i]);
        }
    }

    CONTEXT& context = *pExceptionInfo->ContextRecord;

    Section("REGISTERS");
    {
        const auto DumpRegister = [](auto name, auto value) {
            SPDLOG_INFO("\t{}: {:#010x}", name, value);
        };
        DumpRegister("EAX", context.Eax);
        DumpRegister("EBX", context.Ebx);
        DumpRegister("ECX", context.Ecx);
        DumpRegister("EDX", context.Edx);
        DumpRegister("ESI", context.Esi);
        DumpRegister("EDI", context.Edi);
        DumpRegister("EBP", context.Ebp);
        DumpRegister("ESP", context.Esp);
        DumpRegister("EIP", context.Eip);
        DumpRegister("EFLAGS", context.EFlags);
    }

#if 0
    Section("LOADED MODULES");
    {
        HANDLE hProcess = GetCurrentProcess();

        HMODULE hModules[1024];
        DWORD cbNeeded;

        if (EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded)) {
            const DWORD numModules = cbNeeded / sizeof(HMODULE);
            for (DWORD i = 0; i < numModules; i++) {
                MODULEINFO moduleInfo;
                if (GetModuleInformation(hProcess, hModules[i], &moduleInfo, sizeof(moduleInfo))) {
                    char moduleName[MAX_PATH];
                    GetModuleBaseName (hProcess, hModules[i], moduleName, sizeof(moduleName));

                    SPDLOG_INFO("\t{:#010x}: {}", LOG_PTR(moduleInfo.lpBaseOfDll), moduleName);
                }
            }
        }
    }
#endif

    Section("CALL STACK");
    {
        HANDLE hProcess = GetCurrentProcess();
        HANDLE hThread = GetCurrentThread();

        STACKFRAME stackFrame = {};
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrStack.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Mode = AddrModeFlat;
        stackFrame.AddrPC.Offset = context.Eip;
        stackFrame.AddrStack.Offset = context.Esp;
        stackFrame.AddrFrame.Offset = context.Ebp;

        DWORD prevFrameOffset = 0;
        while (StackWalk(
            IMAGE_FILE_MACHINE_I386, hProcess, hThread, &stackFrame, &context, NULL,
            SymFunctionTableAccess, SymGetModuleBase, NULL))
        {
            NOTSA_LOG_INFO("\t{}", notsa::debug::GetFunctionInfoAtAddress(stackFrame.AddrPC.Offset));
        }
    }

    Section("END OF UNHANDLED EXCEPTION");

    spdlog::apply_all([](auto&& logger) {
        logger->flush();
    });

    return EXCEPTION_EXECUTE_HANDLER;
}

notsa::Logging::Logging() {
    using namespace std::chrono_literals;
    LoadLibraryA("dbghelp.dll"); // преагрузка для MiniDumpWriteDump из VEH (loader lock в контексте исключения)
#if 0
    while (!IsDebuggerPresent()) {
        Sleep(1);
    }
#endif
    spdlog::init_thread_pool(1 << 16, 4);

    // See https://github.com/gabime/spdlog/wiki/3.-Custom-formatting#pattern-flags
    spdlog::set_pattern("%^[%l][%H:%M:%S.%e][%s:%#]: %v%$");
    spdlog::enable_backtrace(128);
    spdlog::set_level(spdlog::level::debug);

    m_sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/log.log"));
    m_sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    spdlog::set_default_logger(Create("default"));
    spdlog::flush_every(100ms);

    // SetUnhandledExceptionFilter(WindowsExceptionHandler); // replaced by minidump last-resort below

    // Last-resort minidump on fatal unhandled exceptions (PR#1448 dumper): dumps/*.dmp
    SetUnhandledExceptionFilter([](PEXCEPTION_POINTERS e) -> LONG {
        notsa::Win32::ExceptionHandler(e);
        return EXCEPTION_EXECUTE_HANDLER;
    });
    // FIRST-PRIORITY vectored handler REMOVED (smoke 2026-10-10): dumping/logging from a VEH
    // on every first-chance AV deadlocks/corrupts (spdlog heap use inside the interrupted
    // thread's context) — the game itself uses first-chance AVs during normal load.
    // Last-resort SUHF minidump above stays: it fires only on truly unhandled exceptions.
    // Minimal AV recorder (smoke 2026-10-10): raw WriteFile only - no heap/log locks, safe in any context.
    // The game's SEH swallows AVs during load and exits via its error path; this captures the site.
    AddVectoredExceptionHandler(1, [](PEXCEPTION_POINTERS e) -> LONG {
        if (e->ExceptionRecord->ExceptionCode != 0xC0000005) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        static volatile LONG s_seen = 0;
        if (const LONG n_seen = InterlockedIncrement(&s_seen); n_seen > 8) {
            return EXCEPTION_CONTINUE_SEARCH;
        } else if (n_seen == 1) {
            // Минидамп на первом читающем AV (не teardown-шум) — оракул для оффлайн-стека
            typedef LONG (WINAPI* MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE, int, void*, void*, void*);
            static MiniDumpWriteDump_t pfnMiniDump = reinterpret_cast<MiniDumpWriteDump_t>(
                GetProcAddress(LoadLibraryA("dbghelp.dll"), "MiniDumpWriteDump"));
            if (pfnMiniDump) {
                if (HANDLE f = CreateFileA("dumps\\veh_first.dmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr); f != INVALID_HANDLE_VALUE) {
                    MINIDUMP_EXCEPTION_INFORMATION mei{};
                    mei.ThreadId          = GetCurrentThreadId();
                    mei.ExceptionPointers = e;
                    mei.ClientPointers    = FALSE;
                    pfnMiniDump(GetCurrentProcess(), GetCurrentThreadId(), f, 2 /*MiniDumpNormal*/,
                                &mei, nullptr, nullptr);
                    CloseHandle(f);
                }
            }
        }
        char buf[256];
        auto w = [](const void* p, DWORD n) {
            HANDLE f = CreateFileA("logs\\veh_av.log", GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, 0, nullptr);
            if (f != INVALID_HANDLE_VALUE) { SetFilePointer(f, 0, nullptr, FILE_END); DWORD wq; WriteFile(f, p, n, &wq, nullptr); CloseHandle(f); }
        };
        int n = _snprintf(buf, sizeof(buf),
            "AV eip=%08X addr=%08X w=%X eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X\n",
            e->ContextRecord->Eip,
            (unsigned)(uintptr_t)e->ExceptionRecord->ExceptionInformation[1],
            (unsigned)e->ExceptionRecord->ExceptionInformation[0],
            e->ContextRecord->Eax, e->ContextRecord->Ebx, e->ContextRecord->Ecx, e->ContextRecord->Edx,
            e->ContextRecord->Esi, e->ContextRecord->Edi, e->ContextRecord->Ebp, e->ContextRecord->Esp);
        w(buf, n);
        { // широкая стопка (32 dword)
            char b2[600];
            int n2 = _snprintf(b2, sizeof(b2), "stk:");
            for (int i = 0; i < 32; i++) {
                n2 += _snprintf(b2 + n2, sizeof(b2) - n2, " %08X", *(DWORD*)(e->ContextRecord->Esp + i * 4));
            }
            n2 += _snprintf(b2 + n2, sizeof(b2) - n2, "\n");
            w(b2, n2);
        }
        { // ebp-цепочка кадров
            DWORD fp = e->ContextRecord->Ebp;
            char b3[512];
            int n3 = _snprintf(b3, sizeof(b3), "frames:");
            for (int i = 0; i < 12 && fp > 0x10000; i++) {
                DWORD* f = (DWORD*)fp;
                if (IsBadReadPtr(f, 8)) break;
                n3 += _snprintf(b3 + n3, sizeof(b3) - n3, " %08X", f[1]);
                fp = f[0];
            }
            n3 += _snprintf(b3 + n3, sizeof(b3) - n3, "\n");
            w(b3, n3);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    });
    // The "silent minute-crash" is NOT an exception (no VEH, no dump): catch terminate/abort too.
    std::set_terminate([]() {
        SPDLOG_ERROR("!!! std::terminate CALLED - capturing context");
        CONTEXT ctx{};
        RtlCaptureContext(&ctx);
        EXCEPTION_POINTERS ep{ nullptr, &ctx };
        notsa::Win32::ExceptionHandler(&ep);
        std::abort();
    });
}

notsa::Logging::~Logging() {
    spdlog::shutdown();
}

auto notsa::Logging::Create(std::string name, std::optional<spdlog::level::level_enum> level) -> notsa::log_ptr {
    auto logger = std::make_shared<spdlog::logger>(name, m_sinks.begin(), m_sinks.end());
    spdlog::initialize_logger(logger);
    if (level.has_value()) {
        logger->set_level(*level);
    }
    return logger;
}
