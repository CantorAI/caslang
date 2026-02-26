#pragma once
#include "xpackage.h"
#include "xlang.h"
#include "base_filter.h"
#include "filter_thread.h"
#include "ifactory.h"
#include "BinPack.h"
#include "help_func.h"
#include "Stats.h"
#include "CasRunner.h"
#include <mutex>
#include <future>
#include <unordered_map>

namespace CasLang
{
    using namespace Galaxy;
	class CasFilter:
		public BaseFilter
	{
		double m_fps = 0;
		Stats m_stats;
		CasLang::CasRunner m_CasRunner;
        
        // Async feedback handling
        std::mutex m_mutex;
        std::unordered_map<unsigned long long, std::promise<X::Value>*> m_pendingRequests;

	public:	
		BEGIN_PACKAGE(CasFilter)
			ADD_BASE(BaseFilter)
			APISET().AddFunc<0>("GetCaps", &CasFilter::GetCaps);
			APISET().AddFunc<0>("GetCombinedPrompts", &CasFilter::GetCombinedPrompts);
		END_PACKAGE
	public:

		void RegMetrics();
		CasFilter();
		CasFilter(const char* libInfo, const char* filterName,IFactory* pFact):
			CasFilter()
		{
			m_libInfo = libInfo;
			m_filterName = filterName;
			m_pFactory = pFact;
			RegMetrics();
		}
		bool onLoad() override;
		virtual bool onPinPutFrame(IPin* pin, X::Value& frame) override;
        virtual void OnFeedback(X::Value stateValue, X::Value feedbackValue) override;
        
		X::Value GetCaps();
		X::Value GetCombinedPrompts();
		void Deliver(X::Value& frame)
		{
			m_stats.OneTime();
			for (const auto& p : *m_outputList)
			{
				X::XPackageValue<Pin> varPin(p);
				Pin& pin = *varPin;
				pin.Put(frame);
			}
			m_fps = m_stats.GetFPS();
		}
		virtual void Run() override;

		virtual X::Value ToBytes() override
		{
			PackBinary pack;
			BaseFilter::Pack(pack);
			return pack.Finish();
		}
		virtual bool FromBytes(X::Value& valBin) override
		{
			PackBinary pack(valBin);
			BaseFilter::Unpack(pack);
			return true;
		}

		virtual long long GetContentSize() override
		{
			return 0;
		}
		virtual bool ToBytes(X::XLStream* pStream) override
		{
			return true;
		}
		virtual bool FromBytes(X::XLStream* pStream) override
		{
			return true;
		}
        
    private:
        bool ParseInputFrame(X::Value& frame, unsigned long long& feedbackId, X::Value& payload, std::string& metaData);
        bool ExtractToolCall(X::Value& payload, std::string& toolName, std::string& callId, X::Dict& args);
        void ProcessRunCall(const std::string& script, const std::string& callId, unsigned long long feedbackId, const std::string& metaData);
        void ProcessGetCapsCall(const std::string& callId, unsigned long long feedbackId);

        X::Value ExecuteExternalTool(const std::string& ns, const std::string& cmd, std::unordered_map<std::string, X::Value>& args, unsigned long long originalFeedbackId, const std::string& metaData);
        void SendToolCall(unsigned long long reqId, const std::string& cmd, X::Value& args, const std::string& metaData);
	};
}
