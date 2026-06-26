from xlang_os import fs
import json
import time

import cantor thru 'lrpc:1000'

galaxy = cantor.GetGalaxy()

_round_finished = False
_last_result = None

def OnEndPointPutFrame(pinName, frame):
    global _round_finished
    global _last_result
    
    # str_meta_data = frame.GetStringFromMeta()
    data = frame.data()
    cantor.log(cantor.LOG_GREEN, "Test Receiver: Got Frame: ", data, cantor.LOG_RESET)
    
    _last_result = data
    _round_finished = True
    return True

def RunPipeline(pipelineFileName):
    cantor.log(cantor.LOG_GREEN, "Checking for pipeline: ", pipelineFileName, cantor.LOG_RESET)
    
    # Check if already running in the Service
    instances = galaxy.QueryPipelineInstancesByFileName(pipelineFileName)
    if instances != None and len(instances) > 0:
        cantor.log(cantor.LOG_GREEN, "Reusing existing Agent Pipeline", cantor.LOG_RESET)
        firstInstanceInfo = instances[0]
        # pipeLineObj is a Remote Object / Proxy
        pipeLineObj = firstInstanceInfo["object"]
        return [pipeLineObj, False]

    # Load new if not found
    cantor.log(cantor.LOG_GREEN, "Loading pipeline via Service...", cantor.LOG_RESET)
    pipeLineObj = galaxy.LoadYamlPipeline(pipelineFileName)
    
    if pipeLineObj is None:
        cantor.log(cantor.LOG_RED, "Error: Galaxy.LoadYamlPipeline returned None.", cantor.LOG_RESET)
        return [None, True]

    pipelineId = pipeLineObj.Id
    cantor.log(cantor.LOG_GREEN, "Loaded Agent Pipeline ID: ", pipelineId, cantor.LOG_RESET)

    pipeLineObj.Run()
    return [pipeLineObj, True]

def SetupAndRunLoop():
    cantor.log(cantor.LOG_GREEN, "--- Starting Test Loop ---", cantor.LOG_RESET)
    
    global _round_finished
    
    strCurFolder = get_module_folder_path()
    curFolder = fs.Folder(strCurFolder)
    strPipelineFileName = curFolder.BuildPath("LLM_Agent_CantorX2.yml")
    
    retInfo = RunPipeline(strPipelineFileName)
    pipelineObj = retInfo[0]
    
    if pipelineObj is None:
        return

    # Connect to Endpoint Filter
    cantor.log(cantor.LOG_GREEN, "Querying endpoints-1...", cantor.LOG_RESET)
    endpointFilter = pipelineObj.QueryFilter("endpoints-1")
    if endpointFilter:
        endpointFilter.setputcallback(OnEndPointPutFrame, None)
    else:
        cantor.log(cantor.LOG_RED, "Error: endpoints-1 filter not found", cantor.LOG_RESET)
        return

    # Connect to Prompt Filter
    cantor.log(cantor.LOG_GREEN, "Querying prompt-1...", cantor.LOG_RESET)
    promptFilter = pipelineObj.QueryFilter("prompt-1")
    if not promptFilter:
        cantor.log(cantor.LOG_RED, "Error: prompt-1 filter not found", cantor.LOG_RESET)
        return
    tools_desc =""
    # Inject CasLang Description
    casFilter = pipelineObj.QueryFilter("CasLangFilter-1")
    if casFilter:
        cas_caps = casFilter.GetCombinedPrompts()
        if cas_caps:
            tools_desc = cas_caps + "\n" + tools_desc
            cantor.log(cantor.LOG_GREEN, "Agent Pipeline: CasLang Caps Injected", cantor.LOG_RESET)

    promptFilter.SetParameter("prompts", tools_desc)

    # Defines tests with expected substrings in output
    tests = [
        {
            "prompt": "count files in D:/CantorAI/caslang",
            "expected": "files" 
        },
        {
            "prompt": "calc sum of files and folders in D:/CantorAI/caslang, need individual count",
            "expected": "files" 
        },
        {
            "prompt": "reverse the string 'CantorAI'",
            "expected": "IAratnaC" 
        },
        {
            "prompt": "calculate average of [10, 20, 30, 40, 50] using caslang",
            "expected": "30"
        },
        {
            "prompt": "Write the text 'HealthCheck' to file 'D:/CantorAI/caslang/test/filter/check.txt' and then read it back",
            "expected": "HealthCheck"
        },
        {
            "prompt": "calculate 100 * 25",
            "expected": "2500"
        }
    ]
    
    for (test,i) in tests:
        prompt = test["prompt"]
        expected = test["expected"]
        
        cantor.log(cantor.LOG_BLUE, f"--- Starting Round {i+1}: {prompt} ---", cantor.LOG_RESET)
        
        _round_finished = False
        _last_result = None # Reset result
        
        meta_data = {
            "sessionId": "test_session_1",
            "userId": "test_user"
        }
        
        # Send Prompt
        promptFilter.Add(prompt, meta_data)
        
        # Wait for completion
        wait_count = 0
        while not _round_finished:
            time.sleep(0.1)
            wait_count += 1
            if wait_count > 1000: # 100 seconds
                cantor.log(cantor.LOG_RED, f"Timeout Waiting for Round {i+1}", cantor.LOG_RESET)
                break
        
        if _round_finished:
            cantor.log(cantor.LOG_GREEN, f"Round {i+1} Finished. Result: {_last_result}", cantor.LOG_RESET)
            
            # Verification
            result_str = str(_last_result)
            if expected in result_str:
                 cantor.log(cantor.LOG_GREEN, f"CONFIRMED: Result contains '{expected}'", cantor.LOG_RESET)
            else:
                 cantor.log(cantor.LOG_RED, f"FAILURE: Result does NOT contain '{expected}'", cantor.LOG_RESET)
                 # Don't exit, try next
        else:
            cantor.log(cantor.LOG_RED, "Round Failed (Timeout)", cantor.LOG_RESET)
        
        time.sleep(1)

    cantor.log(cantor.LOG_GREEN, "Test Suite Finished", cantor.LOG_RESET)

SetupAndRunLoop()
