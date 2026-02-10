#pragma once

#include "value.h"
#include <sstream> 
#include <string>
#include "Locker.h"

namespace CasLang
{
    class Log
    {
        X::Value m_realLogger;
        Locker m_lock;
        std::ostringstream m_buffer;  // Buffer for the current log line

    public:
        Log();
        ~Log();

        void PreInit()
        {
            //do nothing just for this class instance creating
        }
        void Init(X::Value& logger)
        {
            m_realLogger = logger;
        }

        template<typename T>
        inline Log& operator<<(const T& v)
        {
            if (m_level <= m_dumpLevel)
            {
                m_buffer << v;  // Buffer instead of immediate output
            }
            return *this;
        }

        inline void operator<<(Locker* l)
        {
            if (m_level <= m_dumpLevel)
            {
                m_buffer << '\n';
                FlushBuffer();
            }
            l->Unlock();
        }

        Log& SetCurInfo(const char* fileName, const int line, const int level);

        inline Locker* LineEnd()
        {
            return &m_lock;
        }

        inline Locker* End()
        {
            return &m_lock;
        }

        inline void LineBegin()
        {
        }

        inline void SetDumpLevel(int l)
        {
            m_dumpLevel = l;
        }

        inline void SetLevel(int l)
        {
            m_level = l;
        }

    private:
        void FlushBuffer()
        {
            std::string message = m_buffer.str();
            if (!message.empty())
            {
                m_realLogger(message);
            }
            m_buffer.str("");
            m_buffer.clear();
        }

        int m_level = 0;
        int m_dumpLevel = 999999;
    };

    // Construct-on-first-use pattern to avoid static initialization order fiasco
    Log& getLog();

#define PreInitLog(logger) CasLang::getLog().PreInit()
#define InitLog(logger) CasLang::getLog().Init(logger)
#define SetLogSizeLimit(l) CasLang::getLog().SetFileSizeLimit(l)
#define SetLogLevel(l) CasLang::getLog().SetDumpLevel(l)
#define LOGV(level) CasLang::getLog().SetCurInfo(__FILE__,__LINE__,level)
#define LOG LOGV(0)
#define LOG1 LOGV(1)
#define LOG2 LOGV(2)
#define LOG3 LOGV(3)
#define LOG4 LOGV(4)
#define LOG5 LOGV(5)
#define LOG6 LOGV(6)
#define LOG7 LOGV(7)
#define LOG8 LOGV(8)
#define LOG9 LOGV(9)
#define LOG10 LOGV(10)
#define LINE_END CasLang::getLog().LineEnd()
#define LOG_END CasLang::getLog().End()

    // ANSI color codes for console
#define LOG_RED "\033[31m"   // Red color
#define LOG_GREEN "\033[32m" // Green color
#define LOG_YELLOW "\033[33m" // Yellow color
#define LOG_BLUE "\033[34m"  // Blue color
#define LOG_PURPLE  "\033[35m"  // Purple / Magenta
#define LOG_CYAN    "\033[36m"  // Cyan
#define LOG_WHITE   "\033[37m"  // White
#define LOG_GRAY    "\033[90m"  // Gray (bright black)

#define LOG_BRED    "\033[91m"  // Bright Red
#define LOG_BGREEN  "\033[92m"  // Bright Green
#define LOG_BYELLOW "\033[93m"  // Bright Yellow
#define LOG_BBLUE   "\033[94m"  // Bright Blue
#define LOG_BPURPLE "\033[95m"  // Bright Purple
#define LOG_BCYAN   "\033[96m"  // Bright Cyan

#define LOG_RESET "\033[0m"  // Reset to default
}
