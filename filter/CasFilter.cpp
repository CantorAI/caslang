#include "CasFilter.h"
#include "help_func.h"
#include <iostream>
#include <fstream>
#include "port.h"
#include "GalaxyFrame.h"
#include "ActionStringOps.h"
#include "ActionFSOps.h"
#include "ActionNumOps.h"
#include "ActionTimeOps.h"

// Helper function: UTF-8 to UTF-16
std::wstring UTF8ToWString(const std::string& utf8) {
#if (WIN32)
    if (utf8.empty()) return {};
    int wstrLength = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wstrLength <= 0) throw std::runtime_error("MultiByteToWideChar failed.");
    std::wstring wstr(wstrLength - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], wstrLength);
    return wstr;
#else
    throw std::runtime_error("UTF8ToWString is Windows-specific.");
#endif
}

namespace Galaxy
{

    void CasFilter::RegMetrics()
    {
        X::Value cantor = m_pFactory->GetCantor();
        auto metrics_mgr = cantor["Metrics"]();
        auto registerMetrics = metrics_mgr["registerMetrics"];
        X::Func FPS_Fetch("CasFilter_FPS",
            (X::U_FUNC)[this](X::XRuntime* rt, X::XObj* pThis, X::XObj* pContext,
                X::ARGS& params, X::KWARGS& kwParams,
                X::Value& retValue)
            {
                retValue = m_fps;
                return true;
            });
        registerMetrics(cantor,"CasFilter_FPS", FPS_Fetch);

    }
    bool CasFilter::onPinPutFrame(IPin* pin, X::Value& frame)
    {
        if (pin->GetDirection() != PinDirection::Input)
        {
            return false;
        }
        m_stats.OneTime();

        X::XPackageValue<GalaxyFrame> valFrm(frame);
        GalaxyFrame& inputFrame = *valFrm.GetRealObj();
        auto dataFmt = (GalaxyDataFormat)inputFrame.Head()->format[FMT_NUM - 1];
        std::string content;
        if(dataFmt == GalaxyDataFormat::AsRawData)
        {
			content = std::string(inputFrame.Data(), inputFrame.Data() + inputFrame.Head()->dataSize);
		}
        else
        {
            X::XLStream* pStream = X::g_pXHost->CreateStream(inputFrame.Data(), inputFrame.Head()->dataSize);
            X::Value prompts;
            prompts.FromBytes(pStream);
            X::g_pXHost->ReleaseStream(pStream);

            X::Dict dictPrompts(prompts);
            X::Value varRole = dictPrompts->Get("msg_type");
            std::string role = varRole.IsValid() ? varRole.asString() : "user";
            X::Value varContent = dictPrompts->Get("message");
            content = varContent.asString();
        }
        auto results = m_actionRunner.Run(content);

        auto varOutFrm = m_pFactory->NewDataFrame();
        X::XPackage* pPack = dynamic_cast<X::XPackage*>(varOutFrm.GetObj());
        GalaxyFrame* pOutFrm = (GalaxyFrame*)pPack->GetEmbedObj();
        pOutFrm->Head()->type = inputFrame.Head()->type;
        pOutFrm->Head()->uid_l = inputFrame.Head()->uid_l;
        pOutFrm->Head()->uid_h = inputFrame.Head()->uid_h;
        pOutFrm->Head()->sessionId = inputFrame.Head()->sessionId;
        pOutFrm->Head()->startTime = getCurMilliTimeStamp();
        std::string strResults = results.output.asString();
        int retDataSize = (int)strResults.size();
        pOutFrm->Head()->dataSize = retDataSize;
        char* pBuf = pOutFrm->AllocMemory();
        memcpy(pBuf, strResults.data(), retDataSize);

        for (const auto& p : *m_outputList)
        {
            X::XPackageValue<Pin> varPin(p);
            Pin& pin = *varPin;
            pin.Put(varOutFrm);
        }

        m_fps = m_stats.GetFPS();
        return true;
    }
    CasFilter::CasFilter()
    {
        m_filterTypeName = "CasFilter";
        m_actionRunner.Register(std::make_unique<StringOps>());
		m_actionRunner.Register(std::make_unique<ActionFSOps>());
        m_actionRunner.Register(std::make_unique<ActionNumOps>());
        m_actionRunner.Register(std::make_unique<ActionTimeOps>());
    }
    void CasFilter::Run()
    {
        while (m_status == FilterStatus::Running)
        {
            MS_SLEEP(1000);
        }
    }
}
