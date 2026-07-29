#pragma once
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>

// Console debug log — AllocConsole + stdout redirection + timestamps.
// Call tka::LogInit() once at entry, tka::LogShutdown() before FreeLibrary.
namespace tka
{
    inline bool g_LogReady = false;

    inline void LogInit()
    {
        if (g_LogReady) return;
        if (!AllocConsole()) return;
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        SetConsoleTitleW(L"TKA Internal - Debug Log");
        // Disable the console's close button (clicking X would kill the game)
        if (HWND cw = GetConsoleWindow())
        {
            HMENU hm = GetSystemMenu(cw, FALSE);
            if (hm) DeleteMenu(hm, SC_CLOSE, MF_BYCOMMAND);
        }
        g_LogReady = true;
    }

    inline void LogShutdown()
    {
        if (!g_LogReady) return;
        g_LogReady = false;
        fclose(stdout);
        fclose(stderr);
        FreeConsole();
    }

    inline void VLog(const char* tag, const char* fmt, va_list ap)
    {
        if (!g_LogReady) return;
        SYSTEMTIME st{};
        GetLocalTime(&st);
        printf("[%02d:%02d:%02d.%03d][%s] ",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, tag);
        vprintf(fmt, ap);
        printf("\n");
        fflush(stdout);
    }

    inline void Log(const char* fmt, ...)
    {
        va_list ap; va_start(ap, fmt);
        VLog("info", fmt, ap);
        va_end(ap);
    }

    inline void LogOk(const char* fmt, ...)
    {
        va_list ap; va_start(ap, fmt);
        VLog(" ok ", fmt, ap);
        va_end(ap);
    }

    inline void LogErr(const char* fmt, ...)
    {
        va_list ap; va_start(ap, fmt);
        VLog("ERR", fmt, ap);
        va_end(ap);
    }

    inline void LogFeat(const char* fmt, ...)
    {
        va_list ap; va_start(ap, fmt);
        VLog("feat", fmt, ap);
        va_end(ap);
    }
}
