#include "log.h"
#include <chrono>
#include <thread>
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
    m_lock.lock();
    m_level = level;
    if (m_level <= m_dumpLevel)
    {
        m_buffer.str("");  // Clear buffer at start of new line
        m_buffer.clear();

        std::string strFileName(fileName);
        auto pos = strFileName.rfind('/');
        if (pos == std::string::npos) pos = strFileName.rfind('\\');
        if (pos != std::string::npos)
        {
            strFileName = strFileName.substr(pos + 1);
        }
        unsigned long pid = 0; // standard C++ has no getpid without OS headers
        auto tid = std::this_thread::get_id();
        auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        m_buffer << "[" << pid << "-" << tid << "-" << ts
            << "," << strFileName << ":" << line << "] ";
    }
    return *this;
}
