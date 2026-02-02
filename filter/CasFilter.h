#pragma once
#include "xpackage.h"
#include "xlang.h"
#include "base_filter.h"
#include "filter_thread.h"
#include "ifactory.h"
#include "BinPack.h"
#include "help_func.h"
#include "Stats.h"
#include "ActionRunner.h"

namespace Galaxy
{
	class CasFilter:
		public BaseFilter
	{
		double m_fps = 0;
		Stats m_stats;
		ActionRunner m_actionRunner;
	public:	
		BEGIN_PACKAGE(CasFilter)
			ADD_BASE(BaseFilter)

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
		virtual bool onPinPutFrame(IPin* pin, X::Value& frame) override;
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
	};
}
