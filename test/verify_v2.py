import xlang
import os

try:
    print("Importing CasLang...")
    cas = xlang.importModule("caslang", fromPath="caslang")
    
    files = [
        "d:/CantorAI/caslang/test/2_ops/7_expr.cas",
        "d:/CantorAI/caslang/test/2_ops/8_str_v2.cas",
        "d:/CantorAI/caslang/test/2_ops/9_list.cas",
        "d:/CantorAI/caslang/test/2_ops/10_dict_v2.cas",
        "d:/CantorAI/caslang/test/1_flow/4_copy.cas",
        "d:/CantorAI/caslang/test/comprehensive_v2.cas"
    ]
    
    for f in files:
        print(f"Running {os.path.basename(f)}...")
        res = cas.run(f)
        # Workaround: CasLang returns JSON string
        if isinstance(res, str):
            import json
            try:
                res = json.loads(res)
            except Exception as e:
                print(f"JSON Parse Error: {e}")
                print(f"Raw: {res}")
                continue
        if res.get("success"):
            data = res.get("data")
            if not data:
                print("FAILED. Data is empty!")
                continue
            print(f"SUCCESS. Data: {data}")
            # Verify all true
            all_true = True
            if isinstance(data, dict):
                for k,v in data.items():
                    if v is not True and v != 1:
                        all_true = False
                        print(f"  FAILED CHECK: {k}={v}")
            
            if all_true:
                print("  ALL CHECKPOINTS PASSED")
        else:
            print(f"FAILED. Type: {type(res)}")
            print(f"FAILED. Res: {res}")
            print(f"FAILED. Error: {res.get('error')}")

except Exception as e:
    print(f"Exception: {e}")
