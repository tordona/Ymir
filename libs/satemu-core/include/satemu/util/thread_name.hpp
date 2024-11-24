#pragma once

#if defined(_WIN32)

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
        #define LOCALLY_DEFINED_WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
        #define LOCALLY_DEFINED_NOMINMAX
    #endif
    #include <Windows.h>

// Dynamically link to SetThreadDescription
namespace util::detail {

inline struct ThreadDynamicLink {
    ThreadDynamicLink() {
        hKernel32 = LoadLibrary("Kernel32.dll");
        if (hKernel32 != NULL) {
            fnSetThreadDescription = (FnSetThreadDescription)GetProcAddress(hKernel32, "SetThreadDescription");
        } else {
            fnSetThreadDescription = [](HANDLE, PCWSTR) { return E_NOTIMPL; };
        }
    }

    HMODULE hKernel32 = NULL;

    using FnSetThreadDescription = HRESULT(WINAPI *)(HANDLE hThread, PCWSTR lpThreadDescription);
    FnSetThreadDescription fnSetThreadDescription;
} g_threadDynamicLink;

} // namespace util::detail

#elif defined(__linux__)

    #include <pthread.h>

#elif defined(__APPLE__)

    #include <pthread.h>

#endif

namespace util {

inline void SetCurrentThreadName(const char *threadName) {
#if defined(_WIN32)
    // ---------------------------------------------------------------------------------------------
    // Set thread name via exception
    // Works on any Visual Studio version
    const DWORD MS_VC_EXCEPTION = 0x406D1388;

    #pragma pack(push, 8)
    typedef struct tagTHREADNAME_INFO {
        DWORD dwType;     // Must be 0x1000.
        LPCSTR szName;    // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags;    // Reserved for future use, must be zero.
    } THREADNAME_INFO;
    #pragma pack(pop)

    THREADNAME_INFO info{
        .dwType = 0x1000,
        .szName = threadName,
        .dwThreadID = GetCurrentThreadId(),
        .dwFlags = 0,
    };

    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR *)&info);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    // ---------------------------------------------------------------------------------------------
    // Set thread via SetThreadDescription
    // Requires Visual Studio 2017 and later on Windows 10 1607 and later

    // String needs to be converted to LPWSTR
    int length = MultiByteToWideChar(CP_UTF8, 0, threadName, strlen(threadName), NULL, 0);
    if (length > 0) {
        LPWSTR threadNameWstr = new wchar_t[length + 1];
        if (MultiByteToWideChar(CP_UTF8, 0, threadName, strlen(threadName), threadNameWstr, length) == length) {
            threadNameWstr[length] = 0;
            detail::g_threadDynamicLink.fnSetThreadDescription(GetCurrentThread(), threadNameWstr);
        }
        delete[] threadNameWstr;
    }

#elif defined(__linux__)

    pthread_setname_np(pthread_self(), threadName);

#elif defined(__APPLE__)

    pthread_setname_np(threadName);

#endif
}

} // namespace util

#ifdef _WIN32
    #ifdef LOCALLY_DEFINED_WIN32_LEAN_AND_MEAN
        #undef WIN32_LEAN_AND_MEAN
        #undef LOCALLY_DEFINED_WIN32_LEAN_AND_MEAN
    #endif
    #ifdef LOCALLY_DEFINED_NOMINMAX
        #undef NOMINMAX
        #undef LOCALLY_DEFINED_NOMINMAX
    #endif
#endif