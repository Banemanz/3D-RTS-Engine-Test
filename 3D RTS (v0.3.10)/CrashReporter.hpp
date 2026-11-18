#pragma once

// Minimal local crash diagnostics for native Windows faults which cannot be
// caught by C++ exceptions or by GLFW callback guards. Never uploads data.
// Minidump files may contain addresses and limited memory: share privately.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <DbgHelp.h>
#include <cstdio>
namespace RTSCrashReporter {
inline LONG WINAPI onUnhandledException(EXCEPTION_POINTERS* exception) noexcept {
    char name[128]{};
    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::snprintf(name,sizeof(name),"rts-crash-%04u%02u%02u-%02u%02u%02u.dmp",
        unsigned(time.wYear),unsigned(time.wMonth),unsigned(time.wDay),
        unsigned(time.wHour),unsigned(time.wMinute),unsigned(time.wSecond));
    HANDLE file=CreateFileA(name,GENERIC_WRITE,FILE_SHARE_READ,nullptr,
                            CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return EXCEPTION_EXECUTE_HANDLER;
    MINIDUMP_EXCEPTION_INFORMATION context{};
    context.ThreadId=GetCurrentThreadId();
    context.ExceptionPointers=exception;
    context.ClientPointers=FALSE;
    MiniDumpWriteDump(GetCurrentProcess(),GetCurrentProcessId(),file,
                      static_cast<MINIDUMP_TYPE>(MiniDumpNormal|MiniDumpWithThreadInfo|
                                                 MiniDumpWithUnloadedModules),
                      exception?&context:nullptr,nullptr,nullptr);
    CloseHandle(file);
    return EXCEPTION_EXECUTE_HANDLER;
}
inline void install(){SetUnhandledExceptionFilter(onUnhandledException);}
}
#else
namespace RTSCrashReporter {
inline void install(){}
}
#endif
