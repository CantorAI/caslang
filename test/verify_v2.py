import xlang
import os

try:
    print("Importing CasLang...")
    cas = xlang.importModule("caslang", fromPath="caslang")
    
    EXPECTED = {
        "2_ops/7_expr.cas": {"'expr_add'": 1, "'expr_mul'": 1},
        "2_ops/8_str_v2.cas": {"'str_count'": 1},
        "2_ops/9_list.cas": {"'list_len'": 1, "'list_range'": 1},
        "2_ops/10_dict_v2.cas": {"'dict_has'": 1, "'dict_remove'": 1},
        "1_flow/4_copy.cas": {"'deep_copy_orig'": 1, "'deep_copy_clone'": 1},
        "comprehensive_v2.cas": {
            "'expr_add'": 1, "'expr_mul'": 1,
            "'str_count'": 1,
            "'list_len'": 1, "'list_range'": 1,
            "'dict_has'": 1, "'dict_remove'": 1,
            "'deep_copy_orig'": 1, "'deep_copy_clone'": 1
        },
        "11_user_script.cas": {'files': 1, 'folders': 1}
    }

    files = [
        "d:/CantorAI/caslang/test/2_ops/7_expr.cas",
        "d:/CantorAI/caslang/test/2_ops/8_str_v2.cas",
        "d:/CantorAI/caslang/test/2_ops/9_list.cas",
        "d:/CantorAI/caslang/test/2_ops/10_dict_v2.cas",
        "d:/CantorAI/caslang/test/1_flow/4_copy.cas",
        "d:/CantorAI/caslang/test/comprehensive_v2.cas",
        "d:/CantorAI/caslang/test/11_user_script.cas"
    ]
    
    for f in files:
        fname = os.path.basename(f)
        # find key in expected
        key_name = fname
        # Also try relative path conventions if needed, for now use filename matching
        exp = None
        for k,v in EXPECTED.items():
            if k.endswith(fname):
                exp = v
                break

        print(f"Running {fname}...")
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
            
            # Parsing inner JSON if data is string (double serialization case)
            if isinstance(data, str):
                try:
                    data = json.loads(data)
                except:
                    pass # Keep as string if fail

            # Verify against expected
            print(f"SUCCESS. Data: {data}")
            if exp:
                pass_check = True
                for k,v in exp.items():
                    if data.get(k) != v:
                        print(f"  FAILED CHECK: {k}={data.get(k)} (Expected {v})")
                        pass_check = False
                if pass_check:
                    print("  ALL CHECKPOINTS PASSED")
            else:
                 print("  (No specific expectations defined)")
        else:
            print(f"FAILED. Type: {type(res)}")
            print(f"FAILED. Res: {res}")
            print(f"FAILED. Error: {res.get('error')}")

except Exception as e:
    print(f"Exception: {e}")
