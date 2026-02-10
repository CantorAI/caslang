#include "log.h"
#include "port.h"
#include "help_func.h"

namespace CasLang
{
    // Meyer's Singleton - thread-safe in C++11 and later
    // Guarantees Log is constructed before first use, avoiding
    // static initialization order fiasco
    Log& getLog()
    {
        static Log instance;
        return instance;
    }
}

CasLang::Log::Log()
{
}

CasLang::Log::~Log()
{
}

CasLang::Log& CasLang::Log::SetCurInfo(const char* fileName,
    const int line, const int level)
{
    m_lock.Lock();
    m_level = level;
    if (m_level <= m_dumpLevel)
    {
        m_buffer.str("");  // Clear buffer at start of new line
        m_buffer.clear();

        std::string strFileName(fileName);
        auto pos = strFileName.rfind(Path_Sep_S);
        if (pos != std::string::npos)
        {
            strFileName = strFileName.substr(pos + 1);
        }
        unsigned long pid = Galaxy::GetPID();
        unsigned long tid = Galaxy::GetThreadID();
        int64_t ts = Galaxy::getCurTimeStamp();

        m_buffer << "[" << pid << "-" << tid << "-" << ts
            << "," << strFileName << ":" << line << "] ";
    }
    return *this;
}