#include "CasFilter.h"
#include "help_func.h"
#include <iostream>
#include <fstream>
#include "port.h"
#include "GalaxyFrame.h"
#include "CasStringOps.h"
#include "CasFSOps.h"
#include "CasNumOps.h"
#include "CasTimeOps.h"
#include "AgentPrompts.h"

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

namespace CasLang
{
    using namespace Galaxy;

    void CasFilter::RegMetrics()
    {
        X::Value cantor = m_pFactory->GetCantor();
        auto metrics_mgr = cantor["Metrics"]();
        auto registerMetrics = metrics_mgr["registerMetrics"];
        X::Func FPS_Fetch("CasLangFilter_FPS",
            (X::U_FUNC)[this](X::XRuntime* rt, X::XObj* pThis, X::XObj* pContext,
                X::ARGS& params, X::KWARGS& kwParams,
                X::Value& retValue)
            {
                retValue = m_fps;
                return true;
            });
        registerMetrics(cantor,"CasLangFilter_FPS", FPS_Fetch);

    }

    bool CasFilter::onPinPutFrame(IPin* pin, X::Value& frame)
    {
        if (pin->GetDirection() != PinDirection::Input) return false;
        m_stats.OneTime();

        unsigned long long feedbackId = 0;
        X::Value payload;
        if (!ParseInputFrame(frame, feedbackId, payload)) return false;

        std::string toolName, callId;
        X::Dict args;
        if (ExtractToolCall(payload, toolName, callId, args))
        {
            if (toolName == "caslang.run")
            {
                std::string script = args["script"].ToString();
                if (!script.empty())
                {
                    ProcessRunCall(script, callId, feedbackId);
                    return true;
                }
            }
            else if (toolName == "caslang.getcaps")
            {
                ProcessGetCapsCall(callId, feedbackId);
                return true;
            }
        }

        // Pass-through
        Deliver(frame);
        m_fps = m_stats.GetFPS();
        return true;
    }

    bool CasFilter::ParseInputFrame(X::Value& frame, unsigned long long& feedbackId, X::Value& payload)
    {
        X::XPackageValue<GalaxyFrame> valFrm(frame);
        GalaxyFrame& inputFrame = *valFrm.GetRealObj();
        
        feedbackId = inputFrame.Head()->format[0];
        auto dataFmt = (GalaxyDataFormat)inputFrame.Head()->format[FMT_NUM - 1];

        if(dataFmt == GalaxyDataFormat::AsRawData)
        {
			std::string content(inputFrame.Data(), inputFrame.Data() + inputFrame.Head()->dataSize);
             if(content.size()>0 && (content[0] == '[' || content[0] == '{'))
             {
                 X::Runtime rt;
                 X::Package json(rt, "json", "");
                 try{
                    payload = json["loads"](content);
                 } catch(...) { payload = content; }
             }
             else {
                 payload = content;
             }
		}
        else
        {
            X::XLStream* pStream = X::g_pXHost->CreateStream(inputFrame.Data(), inputFrame.Head()->dataSize);
            payload.FromBytes(pStream);
            X::g_pXHost->ReleaseStream(pStream);
        }
        return true;
    }

    bool CasFilter::ExtractToolCall(X::Value& payload, std::string& toolName, std::string& callId, X::Dict& args)
    {
        if (!payload.IsList()) return false;
        
        X::List callList(payload);
        if (callList.Size() == 0) return false;

        X::Dict first(callList[0]);
        toolName = first["name"].ToString();
        callId = first["id"].ToString();
        
        if (first->Has("args"))
        {
            args = first["args"];
        }
        return true;
    }

    void CasFilter::ProcessRunCall(const std::string& script, const std::string& callId, unsigned long long feedbackId)
    {
        // Setup External Handler
        m_CasRunner.SetExternalHandler([this, feedbackId](const std::string& ns, const std::string& cmd, std::unordered_map<std::string, X::Value>& args){
             return this->ExecuteExternalTool(ns, cmd, args, feedbackId);
        });

        auto result = m_CasRunner.Run(script);
        
        if (feedbackId != 0)
        {
             X::Dict retDict;
             retDict->Set("tool_call_id", callId);
             retDict->Set("role", "tool");
             retDict->Set("name", "caslang.run");
             
             if(result.success) {
                 retDict->Set("content", result.output.ToString());
             } else {
                 retDict->Set("content", "Error: " + result.error);
             }
             
             X::List retList;
             retList += retDict;
             
             IssueFeedback(feedbackId, retList);
        }
    }

    void CasFilter::ProcessGetCapsCall(const std::string& callId, unsigned long long feedbackId)
    {
        if (feedbackId == 0) return;

        // 1. Get CasLang Caps (Prompts)
        X::Dict caps;
        X::List listPrompts;
        for(size_t i=0; i<kPromptsCount; i++)
        {
            X::Dict p;
            p->Set("role", kPrompts[i].role);
            p->Set("content", kPrompts[i].content);
            listPrompts += p;
        }
        
        // 2. Query Downstream (Action) Caps
        std::unordered_map<std::string, X::Value> args; // empty
        X::Value actionCapsVal = ExecuteExternalTool("action", "getcaps", args, feedbackId);
        
        // actionCapsVal should be the JSON string of the registry/tools
        if(actionCapsVal.IsValid() && actionCapsVal.IsString())
        {
            std::string actionJson = actionCapsVal.ToString();
            // Parse? Or just wrap. 
            // Registry is likely a list of definitions or a Dict.
            // Let's assume we just append it to a "tools" section or merge?
            // "caps" usually implies system prompts + tool definitions.
            // LlmAgent expects what?
            // If we assume standard Galaxy agent format:
            // return { "caps": [prompt_objects...], "tools": [tool_defs...] } ?
            // Or just mixing them?
            // User requested "return caps for the LLM".
            // CasLang Prompt is a system message.
            // Action caps are tool definitions.
            // Let's store Action caps in "tools" key if parsed, or just append raw string if we can't parse?
            // Better to try parse.
             X::Runtime rt;
             X::Package json(rt, "json", "");
             try {
                X::Value rules = json["loads"](actionJson);
                caps->Set("tools", rules);
             } catch(...) {
                caps->Set("tools_raw", actionJson);
             }
        }
        
        caps->Set("caps", listPrompts);
        
        X::Dict retDict;
        retDict->Set("tool_call_id", callId);
        retDict->Set("role", "tool");
        retDict->Set("name", "caslang.getcaps");
        retDict->Set("content", caps.ToString());
        
        X::List retList;
        retList += retDict;
        
        IssueFeedback(feedbackId, retList);
    }

    X::Value CasFilter::ExecuteExternalTool(const std::string& ns, const std::string& cmd, std::unordered_map<std::string, X::Value>& args, unsigned long long originalFeedbackId)
    {
        X::Value stateDict; 
        X::Dict d(stateDict); 
        unsigned long long internalId = (unsigned long long)this + getCurMilliTimeStamp() + (unsigned long long)&d;
        d->Set("internal_id", internalId);
        
        unsigned long long reqId = RegisterFeedback(stateDict);
        
        std::promise<X::Value> prom;
        auto fut = prom.get_future();
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingRequests[internalId] = &prom; // Map Internal ID
        }

        SendToolCall(reqId, ns, cmd, args);

        // Wait with simple timeout safety (e.g. 60 sec) or infinite?
        // Agent tools can take time. Infinite for now.
        fut.wait();
        X::Value result = fut.get();
        
        return result;
    }

    void CasFilter::SendToolCall(unsigned long long reqId, const std::string& ns, const std::string& cmd, std::unordered_map<std::string, X::Value>& args)
    {
         X::Dict callObj;
         std::string fullName = ns + "." + cmd;
         callObj->Set("name", fullName);
         
         X::Dict argsDict;
         for(auto& kv : args)
         {
             argsDict->Set(kv.first, kv.second);
         }
         callObj->Set("args", argsDict);
         callObj->Set("id", std::to_string(reqId));
         
         X::List calls;
         calls += callObj;
         
         auto varOutFrm = m_pFactory->NewDataFrame();
         X::XPackage* pPack = dynamic_cast<X::XPackage*>(varOutFrm.GetObj());
         GalaxyFrame* pOutFrm = (GalaxyFrame*)pPack->GetEmbedObj();
         
         pOutFrm->Head()->startTime = getCurMilliTimeStamp();
         
         X::XLStream* pStream = X::g_pXHost->CreateStream();
         calls.ToBytes(pStream);
         int size = (int)pStream->Size();
         
         pOutFrm->Head()->dataSize = size;
         pOutFrm->Head()->format[0] = reqId; 
         pOutFrm->Head()->format[FMT_NUM - 1] = (unsigned long long)GalaxyDataFormat::AsXValue; 
         
         char* pBuf = pOutFrm->AllocMemory();
         pStream->FullCopyTo(pBuf, size);
         X::g_pXHost->ReleaseStream(pStream);
         
        for (const auto& p : *m_outputList)
        {
            X::XPackageValue<Pin> varPin(p);
            Pin& pin = *varPin;
            pin.Put(varOutFrm);
        }
    }

    void CasFilter::OnFeedback(X::Value stateValue, X::Value feedbackValue)
    {
        // IssueFeedback from Action sends 'results' which is a List of Tool Calls results.
        // We typically sent 1 tool call.
        X::Value singleResult;
        if(feedbackValue.IsList())
        {
            X::List l(feedbackValue);
            if(l.Size()>0) 
            {
                X::Dict d(l[0]);
                singleResult = d["content"];
            }
        }
        else singleResult = feedbackValue;

        // How to get ID?
        // Galaxy Factory keeps map: ID -> {Filter, State}.
        // When it calls OnFeedback(state, value), it doesn't pass ID explicitly in params.
        // But we need the ID to find the Promise!
        // Solution: We must store the ID *IN* the State object when calling RegisterFeedback.
        
        // Wait, looking at ExecuteExternalTool, I passed 'stateDict'. I should set ID in it.
        // But I don't know ID before calling RegisterFeedback!
        // Catch-22? 
        // Usually RegisterFeedback returns an ID. We can't put it in State before calling.
        // However, we can use a custom UUID or pointer address as a key in State, 
        // and map that to Promise?
        // OR: Galaxy might support updating state? No.
        
        // Alternative: If 'stateValue' is the exact object we passed in, we can use its property.
        // But if we didn't put anything unique...
        // Let's retry:
        // Create a UUID/Random/Pointer. Put in State. Call RegisterFeedback.
        // OnFeedback checks State[UUID]. Finds Promise.
        // The returned reqId from RegisterFeedback is for external correlation, we don't strictly need it for internal lookup if we have State.
        
        // Let's assume stateValue is passed back.
        // I will fix ExecuteExternalTool to put 'internal_id' in state.
        
        X::Dict state(stateValue);
        if(state->Has("internal_id"))
        {
            unsigned long long ptrVal = (unsigned long long)state["internal_id"]; 
            
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_pendingRequests.find(ptrVal);
            if(it != m_pendingRequests.end())
            {
                it->second->set_value(singleResult);
                m_pendingRequests.erase(it);
            }
        }
    }

    X::Value CasFilter::GetCaps()
    {
        X::Dict caps;
        X::List listPrompts;
        for(size_t i=0; i<kPromptsCount; i++)
        {
            X::Dict p;
            p->Set("role", kPrompts[i].role);
            p->Set("content", kPrompts[i].content);
            listPrompts += p;
        }
        caps->Set("caps", listPrompts);
        return caps;
    }

    X::Value CasFilter::GetCombinedPrompts()
    {
        std::string allPrompts;
        for(size_t i=0; i<kPromptsCount; i++)
        {
            if(i > 0) allPrompts += "\n";
            allPrompts += kPrompts[i].content;
        }
        return allPrompts;
    }

    CasFilter::CasFilter()
    {
        m_filterTypeName = "CasLangFilter";
        m_CasRunner.Register(std::make_unique<CasLang::CasStringOps>());
		m_CasRunner.Register(std::make_unique<CasLang::CasFSOps>());
        m_CasRunner.Register(std::make_unique<CasLang::CasNumOps>());
        m_CasRunner.Register(std::make_unique<CasLang::CasTimeOps>());
    }
    void CasFilter::Run()
    {
        while (m_status == FilterStatus::Running)
        {
            MS_SLEEP(1000);
        }
    }
}
