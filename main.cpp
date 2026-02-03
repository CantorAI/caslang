#include "xhost.h"
#include "xpackage.h"
#include "core/CasLang.h"

#if (WIN32)
#include <windows.h>
#define X_EXPORT __declspec(dllexport) 
#else
#define X_EXPORT
#endif

namespace X
{
	XHost* g_pXHost = nullptr;
}

static bool GetCurLibInfo(void* EntryFuncName, std::string& strFullPath,
	std::string& strFolderPath, std::string& strLibName)
{
#if (WIN32)
	HMODULE  hModule = NULL;
	GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		(LPCTSTR)EntryFuncName,
		&hModule);
	char path[MAX_PATH];
	GetModuleFileName(hModule, path, MAX_PATH);
	std::string strPath(path);
	strFullPath = strPath;
	auto pos = strPath.rfind("\\");
	if (pos != std::string::npos)
	{
		strFolderPath = strPath.substr(0, pos);
		strLibName = strPath.substr(pos + 1);
	}
#else
	Dl_info dl_info;
	dladdr((void*)EntryFuncName, &dl_info);
	std::string strPath = dl_info.dli_fname;
	strFullPath = strPath;
	auto pos = strPath.rfind("/");
	if (pos != std::string::npos)
	{
		strFolderPath = strPath.substr(0, pos);
		strLibName = strPath.substr(pos + 1);
	}
#endif
	//remove ext
	pos = strLibName.rfind(".");
	if (pos != std::string::npos)
	{
		strLibName = strLibName.substr(0, pos);
	}
	return true;
}

// Xlang Module Entry
extern "C" X_EXPORT void Load(void* pHost,X::Value curModule)
{
	std::string strFullPath;
	std::string strFolderPath;
	std::string strLibName;
	GetCurLibInfo((void*)Load, strFullPath, strFolderPath, strLibName);
	X::g_pXHost = (X::XHost*)pHost;

    // Register CasLang
    X::RegisterPackage<CasLang::CasLangModule>(strFullPath.c_str(), "caslang", &CasLang::CasLangModule::I());
}

#ifdef BUILD_GALAXY_FILTER
#include "filter/CasFilter.h"
// Galaxy Filter Entry
extern "C"  X_EXPORT void GLoad(const char* libInfo,const char* filterName,
	void* pXlangHost,void* pFactory,X::Value& varFilter)
{
	std::string strFullPath;
	std::string strFolderPath;
	std::string strLibName;
	GetCurLibInfo((void *)GLoad, strFullPath, strFolderPath, strLibName);

	X::g_pXHost = (X::XHost*)pXlangHost;
    
    std::string strFilterName(filterName);
    if (strFilterName == "CasLangFilter")
    {
        // Register under CasLangFilter alias, but class is CasLang::CasFilter
        X::RegisterPackage<CasLang::CasFilter>(strLibName.c_str(), "CasLangFilter");
        // IFactory is still Galaxy::IFactory? Let's assume yes as it comes from framework.
        CasLang::CasFilter* pFilter = new CasLang::CasFilter(libInfo, filterName, (Galaxy::IFactory*)pFactory);
        varFilter = X::Value(pFilter->APISET().GetProxy(pFilter), false);
    }
}
#endif

extern "C"  X_EXPORT void Unload()
{
	X::g_pXHost = nullptr;
}
