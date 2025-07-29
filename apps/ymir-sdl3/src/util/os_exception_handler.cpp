#include "os_exception_handler.hpp"

#include <SDL3/SDL_messagebox.h>

#include <fmt/format.h>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
#elif defined(__linux__)
    #include <signal.h>

    #include <bits/types/siginfo_t.h>
#elif defined(__APPLE__)
// TODO: exception handling
#endif

#include <ymir/core/types.hpp>

namespace util {

// -----------------------------------------------------------------------------
// Windows implementation

#ifdef _WIN32

static PVOID g_vehPtr = nullptr;

void RegisterExceptionHandler() {
    g_vehPtr = AddVectoredExceptionHandler(1, [](_EXCEPTION_POINTERS *ExceptionInfo) -> LONG {
        fmt::memory_buffer buf{};
        auto out = std::back_inserter(buf);

        fmt::format_to(out, "Ymir encountered a fatal error.\n\n");
        fmt::format_to(out, "Exception code=0x{:X} address={} flags=0x{:X}\n\n",
                       ExceptionInfo->ExceptionRecord->ExceptionCode, ExceptionInfo->ExceptionRecord->ExceptionAddress,
                       ExceptionInfo->ExceptionRecord->ExceptionFlags);

        auto *cr = ExceptionInfo->ContextRecord;
        fmt::format_to(out, "Content information:\n");

    #if defined(_M_X64) || defined(__x86_64__)

        fmt::format_to(out, "RAX={:016X} RBX={:016X} RCX={:016X} RDX={:016X}\n", cr->Rax, cr->Rbx, cr->Rcx, cr->Rdx);
        fmt::format_to(out, "RSP={:016X} RBP={:016X} RSI={:016X} RDI={:016X}\n", cr->Rsp, cr->Rbp, cr->Rsi, cr->Rdi);
        fmt::format_to(out, "R8={:016X} R9={:016X} R10={:016X} R11={:016X}\n", cr->R8, cr->R9, cr->R10, cr->R11);
        fmt::format_to(out, "R12={:016X} R13={:016X} R14={:016X} R15={:016X}\n", cr->R12, cr->R13, cr->R14, cr->R15);
        fmt::format_to(out, "CS={:02X} DS={:02X} ES={:02X} FS={:02X} GS={:02X} SS={:02X}\n", cr->SegCs, cr->SegDs,
                       cr->SegEs, cr->SegFs, cr->SegGs, cr->SegSs);
        fmt::format_to(out, "RIP={:016X} EFlags={:08X} MXCSR={:08X} ContextFlags={:08X}", cr->Rip, cr->EFlags,
                       cr->MxCsr, cr->ContextFlags);

    #elif defined(_M_ARM64) || defined(__aarch64__)

    fmt::format_to(out, "R0={:016X} R1={:016X} R2={:016X} R3={:016X}\n", cr->R0, cr->R1, cr->R2, cr->R3);
    fmt::format_to(out, "R4={:016X} R5={:016X} R6={:016X} R7={:016X}\n", cr->R4, cr->R5, cr->R6, cr->R7);
    fmt::format_to(out, "R8={:016X} R9={:016X} R10={:016X} R11={:016X}\n", cr->R8, cr->R9, cr->R10, cr->R11);
    fmt::format_to(out, "R12={:016X}\n", cr->R12);
    fmt::format_to(out, "SP={:X} LR={:X} PC={:X} CPSR={:X}", cr->Sp, cr->Lr, cr->Pc, cr->Cpsr);

    #endif

        std::string errMsg = fmt::to_string(buf);

        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error", errMsg.c_str(), nullptr);

        return EXCEPTION_EXECUTE_HANDLER;
    });
}

/*void UnregisterExceptionHandler() {
    RemoveVectoredExceptionHandler(g_vehPtr);
    g_vehPtr = nullptr;
}*/

#elif defined(__linux__)

// -----------------------------------------------------------------------------
// Linux implementation

// static struct sigaction s_oldAction;

void RegisterExceptionHandler() {
    struct sigaction action;
    action.sa_sigaction = [](int sig, siginfo_t *info, void *ucontext) -> void {
        const auto addr = reinterpret_cast<uintptr_t>(info->si_addr);
        auto *context = static_cast<ucontext_t *>(ucontext);
        auto *mcontext = context->uc_mcontext;

        fmt::memory_buffer buf{};
        auto out = std::back_inserter(buf);

        fmt::format_to(out, "Ymir encountered a fatal error.\n\n");
        fmt::format_to(out, "signo=0x{:X} code=0x{:X} address=0x{:X}\n", info->si_signo, info->si_code, addr);

        // TODO: signal-specific info
        fmt::format_to(out, "\n");

        fmt::format_to(out, "Content information:\n");

    #if defined(_M_X64) || defined(__x86_64__)

        auto *gregs = mcontext.gregs;
        fmt::format_to(out, "RAX={:016X} RBX={:016X} RCX={:016X} RDX={:016X}\n", gregs[REG_RAX], gregs[REG_RBX],
                       gregs[REG_RCX], gregs[REG_RDX]);
        fmt::format_to(out, "RSP={:016X} RBP={:016X} RSI={:016X} RDI={:016X}\n", gregs[REG_RSP], gregs[REG_RBP],
                       gregs[REG_RSI], gregs[REG_RDI]);
        fmt::format_to(out, "R8={:016X} R9={:016X} R10={:016X} R11={:016X}\n", gregs[REG_R8], gregs[REG_R9],
                       gregs[REG_R10], gregs[REG_R11]);
        fmt::format_to(out, "R12={:016X} R13={:016X} R14={:016X} R15={:016X}\n", gregs[REG_R12], gregs[REG_R13],
                       gregs[REG_R14], gregs[REG_R15]);
        fmt::format_to(out, "CSFSGS={:016X} RIP={:016X} EFlags={:08X}", gregs[REG_CSGSFS], gregs[REG_RIP],
                       gregs[REG_EFL]);

    #elif defined(_M_ARM64) || defined(__aarch64__)

        auto *regs = mcontext.regs;
        fmt::format_to(out, "R0={:016X} R1={:016X} R2={:016X} R3={:016X}\n", regs[0], regs[1], regs[2], regs[3]);
        fmt::format_to(out, "R4={:016X} R5={:016X} R6={:016X} R7={:016X}\n", regs[4], regs[5], regs[6], regs[7]);
        fmt::format_to(out, "R8={:016X} R9={:016X} R10={:016X} R11={:016X}\n", regs[8], regs[9], regs[10], regs[11]);
        fmt::format_to(out, "R12={:016X} R13={:016X} R14={:016X} R15={:016X}\n", regs[12], regs[13], regs[14],
                       regs[15]);
        fmt::format_to(out, "R16={:016X} R17={:016X} R18={:016X} R19={:016X}\n", regs[16], regs[17], regs[18],
                       regs[19]);
        fmt::format_to(out, "R20={:016X} R21={:016X} R22={:016X} R23={:016X}\n", regs[20], regs[21], regs[22],
                       regs[23]);
        fmt::format_to(out, "R24={:016X} R25={:016X} R26={:016X} R27={:016X}\n", regs[24], regs[25], regs[26],
                       regs[27]);
        fmt::format_to(out, "R28={:016X} R29={:016X} R30={:016X}\n", regs[28], regs[29], regs[30]);
        fmt::format_to(out, "SP={:X} PC={:X} pstate={:X}", mcontext.sp, mcontext.pc, mcontext.pstate);

    #endif

        std::string errMsg = fmt::to_string(buf);

        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error", errMsg.c_str(), nullptr);

        /*if (s_oldAction.sa_handler) {
            s_oldAction.sa_handler(sig);
        } else if (s_oldAction.sa_sigaction) {
            s_oldAction.sa_sigaction(sig, info, ucontext);
        }*/
        abort();
    };
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
}

#elif defined(__APPLE__)

// -----------------------------------------------------------------------------
// macOS implementation

void RegisterExceptionHandler() {
    // TODO: implement
}

#endif

} // namespace util
