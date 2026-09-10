#pragma once
#include <stdio.h>
#include <shared_mutex>

// Error(): always written to Data\F4SE\Plugins\cbp.log (load, hook, resets).
// Info():  per-frame chatter; compiled out unless CBP_VERBOSE_LOG is defined.
class CbpLogger
{
public:
    CbpLogger(const char* fname);
    ~CbpLogger();
    void Info(const char* args...);
    void Error(const char* args...);

    FILE* handle;

    std::shared_mutex log_lock;
};

extern CbpLogger logger;
