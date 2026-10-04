#include "StdInc.h"

#include "app_debug.h"
#include <windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <DbgHelp.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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

// NOTSA(bisection/ovfix): SEH-walkable EBP chain walker (no C++ objects -> __try is allowed here)
static void WalkEbpChain(DWORD* ebp) {
    DWORD lastRet    = 0;
    int  repeatCount = 0;
    for (int i = 0; i < 20000 && ebp; ++i) {
        DWORD ret = 0, next = 0;
        __try {
            ret  = ebp[1];
            next = ebp[0];
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SPDLOG_INFO("\t[chain read fault at frame {}]", i);
            return;
        }
        // Compress long recursion runs: log first 8, then "... x N same ...", then the transition + tail
        if (ret == lastRet && repeatCount < 8) {
            SPDLOG_INFO("\t[{:05d}] ebp={:#010x} ret={:#010x}", i, (DWORD)(uintptr_t)ebp, ret);
            ++repeatCount;
        } else if (ret == lastRet) {
            if (repeatCount == 8)
                SPDLOG_INFO("\t... frames with same ret={} ...", (void*)(uintptr_t)ret);
            ++repeatCount;
        } else {
            if (repeatCount > 8)
                SPDLOG_INFO("\t[{:05d}] total {} frames with ret={:#010x}", i, repeatCount, lastRet);
            HMODULE m = NULL;
            char name[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)(uintptr_t)ret, &m)) {
                GetModuleFileNameA(m, name, MAX_PATH);
                char* base = strrchr(name, '\\');
                memmove(name, base ? base + 1 : name, strlen(base ? base + 1 : name) + 1);
            }
            SPDLOG_INFO("\t[{:05d}] ebp={:#010x} ret={:#010x} ({})", i, (DWORD)(uintptr_t)ebp, ret, name);
            repeatCount = 1;
        }
        lastRet = ret;
        if (!next || next <= (DWORD)(uintptr_t)ebp || next - (DWORD)(uintptr_t)ebp > 0x10000) {
            SPDLOG_INFO("\t[chain end at frame {}]", i);
            return;
        }
        ebp = (DWORD*)(uintptr_t)next;
    }
}

// NOTSA(bisection/ovfix): crash-dump context handed to the dump thread (a fresh 4MB stack,
// since after a stack overflow the faulting thread has ~1 page of stack left - not enough
// for logging/walking, which then dies mid-way and truncates the log).
struct CrashDumpCtx {
    CONTEXT                  Ctx{};
    unsigned long            esp[64]{};
    std::vector<std::string> frames;
};
static CrashDumpCtx g_CrashDumpCtx;
static HANDLE       g_CrashDumpDone = NULL;

static void DumpCrashDetails(CrashDumpCtx* c) {
    const auto Section2 = [](const char* name) {
        SPDLOG_INFO("*********{}**********", name);
    };

    Section2("RAW STACK (ESP, 64 words)");
    {
        for (int i = 0; i < 64; ++i) {
            const auto v = c->esp[i];
            HMODULE m = NULL;
            char name[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)(uintptr_t)v, &m)) {
                GetModuleFileNameA(m, name, MAX_PATH);
                char* base = strrchr(name, '\\');
                memmove(name, base ? base + 1 : name, strlen(base ? base + 1 : name) + 1);
            }
            SPDLOG_INFO("	[{:02d}] {:#010x} ({}) base={:#010x}", i, v, name, (DWORD)(uintptr_t)m);
        }
    }

    // NOTSA(bisection/ovfix): manual EBP-chain walk - StackWalk dies when the innermost
    // frames have EBP==0 (or are FPO), and for stack overflows the interesting frames are
    // DEEP (hundreds of recursion levels above), beyond any raw-stack window.
    Section2("EBP CHAIN (deep)");
    WalkEbpChain((DWORD*)c->Ctx.Ebp);

    Section2("CALL STACK");
    {
        HANDLE hProcess = GetCurrentProcess();
        HANDLE hThread  = GetCurrentThread();

        // Initialize symbol handler
        SymInitialize(hProcess, NULL, TRUE);

        STACKFRAME stackFrame = {};
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrStack.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Mode = AddrModeFlat;
        stackFrame.AddrPC.Offset = c->Ctx.Eip;
        stackFrame.AddrStack.Offset = c->Ctx.Esp;
        stackFrame.AddrFrame.Offset = c->Ctx.Ebp;

        DWORD prevFrameOffset = 0;
        int nFrames = 0;
        while (nFrames++ < 64 && StackWalk(
            IMAGE_FILE_MACHINE_I386, hProcess, hThread, &stackFrame, &c->Ctx, NULL,
            SymFunctionTableAccess, SymGetModuleBase, NULL))
        {
            DWORD pcOffset = stackFrame.AddrPC.Offset;
            if (pcOffset == 0 || pcOffset == prevFrameOffset) {
                break;
            }
            prevFrameOffset = pcOffset;
            DWORD moduleBase = SymGetModuleBase(hProcess, pcOffset);
            if (moduleBase == NULL) {
                // Module not resolvable (eg. dynamically mapped code) - log the raw
                // frame and keep walking instead of aborting the whole walk.
                SPDLOG_INFO("\t{:#010x}: <module unknown>", pcOffset);
                continue;
            }

            DWORD displacement = 0;
            IMAGEHLP_LINE lineInfo = { sizeof(IMAGEHLP_LINE) };
            BOOL hasLineInfo = SymGetLineFromAddr(hProcess, pcOffset, &displacement, &lineInfo);

            IMAGEHLP_MODULE moduleInfo{ sizeof(IMAGEHLP_MODULE) };
            BOOL hasModuleInfo = SymGetModuleInfo(hProcess, moduleBase, &moduleInfo);

            char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = { 0 };
            PSYMBOL_INFO sym = (PSYMBOL_INFO)symbolBuffer;
            sym->SizeOfStruct = sizeof(SYMBOL_INFO);
            sym->MaxNameLen = MAX_SYM_NAME;
            BOOL hasSym = SymFromAddr(hProcess, pcOffset, NULL, sym);

            char frame[512];
            sprintf_s(frame, "%s!%s+%d", hasModuleInfo ? moduleInfo.ModuleName : "<unknown>",
                      hasSym ? sym->Name : "<unknown>", displacement);
            SPDLOG_INFO("\t{:#010x}: {} (base={:#010x})", pcOffset, frame, moduleBase);
            c->frames.emplace_back(frame);
        }

        // Cleanup symbol handler
        SymCleanup(hProcess);
    }
}

static DWORD WINAPI CrashDumpThreadProc(LPVOID param) {
    __try {
        DumpCrashDetails((CrashDumpCtx*)param);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SPDLOG_INFO("[crashdump] dump thread faulted");
    }
    SetEvent(g_CrashDumpDone);
    return 0;
}

// This probably should be in winps :D
LONG WINAPI WindowsExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    // NOTSA(stability): benign debugger notifications must pass through
    // untouched and unlogged (each log() allocates -> can deadlock the heap
    // with other threads = the notorious 'freeze').
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == 0x4001000A /* DBG_PRINTEXCEPTION_C */
     || pExceptionInfo->ExceptionRecord->ExceptionCode == 0x40010006 /* DBG_PRINTEXCEPTION_WIDE_C */) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // If this function itself crashes it's invoked again.
    // Prevent recursion, but DO NOT swallow the exception: pass it on, so
    // real AVs crash with a dump instead of freezing in a dispatch loop.
    static bool s_HasHandled = false;
    if (s_HasHandled) {
        return EXCEPTION_CONTINUE_SEARCH;
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

    Section("Crash details dumped on a dedicated thread (see below)");

    // NOTSA(bisection/ovfix): after a stack overflow there's ~1 page of stack left on the
    // faulting thread - logging/walking dies mid-way (truncated log). Run ALL heavy dumps
    // (raw stack, deep EBP chain, StackWalk) on a dedicated thread with a fresh 4MB stack.
    {
        g_CrashDumpCtx = CrashDumpCtx{};
        g_CrashDumpCtx.Ctx = context;
        DWORD* esp = (DWORD*)context.Esp;
        for (int i = 0; i < 64; ++i) {
            g_CrashDumpCtx.esp[i] = esp[i];
        }
        g_CrashDumpDone = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        HANDLE hThread = CreateThread(nullptr, 4 * 1024 * 1024, CrashDumpThreadProc, &g_CrashDumpCtx, 0, nullptr);
        if (hThread) {
            WaitForSingleObject(g_CrashDumpDone, 60000);
            CloseHandle(hThread);
            CloseHandle(g_CrashDumpDone);
        } else { // couldn't create a thread (out of memory etc) - just do it inline and hope
            DumpCrashDetails(&g_CrashDumpCtx);
        }
    }

    Section("END OF UNHANDLED EXCEPTION");

    // NOTSA(test): show the culprit on screen (top 3 stack frames) so manual
    // testers instantly see which function crashed.
    {
        std::string top3;
        for (size_t i = 0; i < g_CrashDumpCtx.frames.size() && i < 3; ++i) {
            top3 += g_CrashDumpCtx.frames[i] + "\n";
        }
        char mb[1024];
        sprintf_s(mb, "CRASH 0x%08X @ 0x%08X\n\n%s",
                  pExceptionInfo->ExceptionRecord->ExceptionCode,
                  (DWORD)(uintptr_t)pExceptionInfo->ExceptionRecord->ExceptionAddress,
                  top3.c_str());
        // NOTSA(headless): a modal MessageBox offscreen turns any crash into an
        // invisible "freeze" (nothing can click OK). In headless mode log only
        // and let the process die so the test harness reports a real crash.
        if (!GetEnvironmentVariableA("NOTSA_HEADLESS", nullptr, 0)) {
            MessageBoxA(nullptr, mb, "gta-reversed crash (culprit above)", MB_ICONERROR | MB_OK);
        }
    }

    spdlog::apply_all([](auto&& logger) {
        logger->flush();
    });

    // NOTSA(stability): this is a VECTORED handler (AddVectoredExceptionHandler).
    // Returning EXCEPTION_EXECUTE_HANDLER from a VEH means CONTINUE_EXECUTION,
    // which resumes at the faulting instruction -> infinite exception dispatch
    // loop = the game "freezes" instead of crashing. Always pass it on.
    return EXCEPTION_CONTINUE_SEARCH;
}

notsa::Logging::Logging() {
    using namespace std::chrono_literals;
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
    // NOTSA(stability): no stdout sink. The console window (a) lets Quick Edit
    // text selection block every log write = total game freeze, (b) is useless
    // to the player - everything is in logs/log.log.
    if (GetConsoleWindow()) {
        FreeConsole();
    }

    spdlog::set_default_logger(Create("default"));
    spdlog::flush_every(100ms);
    
    AddVectoredExceptionHandler(1, WindowsExceptionHandler);
}

notsa::Logging::~Logging() {
    //spdlog::shutdown(); // some kind of deadlock occurs and it never shuts down
}

auto notsa::Logging::Create(std::string name, std::optional<spdlog::level::level_enum> level) -> notsa::log_ptr {
    auto logger = std::make_shared<spdlog::logger>(name, m_sinks.begin(), m_sinks.end());
    spdlog::initialize_logger(logger);
    if (level.has_value()) {
        logger->set_level(*level);
    }
    return logger;
}
