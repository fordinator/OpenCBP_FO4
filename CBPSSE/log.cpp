#include <stdarg.h>
#include <mutex>
#include "log.h"

#pragma warning(disable : 4996)

CbpLogger::CbpLogger(const char* fname)
    : handle(nullptr)
{
    handle = fopen(fname, "w");
    if (handle)
    {
        fprintf(handle, "OpenCBP log initialized\n");
        fflush(handle);
    }
}

CbpLogger::~CbpLogger()
{
    if (handle)
    {
        fclose(handle);
        handle = nullptr;
    }
}

void CbpLogger::Info(const char* fmt...)
{
#ifdef CBP_VERBOSE_LOG
    if (handle)
    {
        std::unique_lock<std::shared_mutex> lock(log_lock);
        va_list argptr;
        va_start(argptr, fmt);
        vfprintf(handle, fmt, argptr);
        va_end(argptr);
        fflush(handle);
    }
#else
    (void)fmt;
#endif
}

void CbpLogger::Error(const char* fmt...)
{
    if (handle)
    {
        std::unique_lock<std::shared_mutex> lock(log_lock);
        va_list argptr;
        va_start(argptr, fmt);
        vfprintf(handle, fmt, argptr);
        va_end(argptr);
        fflush(handle);
    }
}

CbpLogger logger("Data\\F4SE\\Plugins\\cbp.log");
