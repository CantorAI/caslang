import xlang
import os
import json

try:
    print("Attempting to import caslang module...")
    caslang_mod = xlang.importModule("caslang", fromPath="caslang")
    print("CasLang imported successfully")
    
    script_dir = "D:/CantorAI/caslang/test"
    
    # 1. Normal Tests (Expected Success)
    normal_tests = [
        "0_general/1_hello.cas",
        "0_general/2_vars.cas",
        "1_flow/1_if.cas",
        "1_flow/2_loop.cas",
        "1_flow/3_retry.cas",
        "2_ops/1_str.cas",
        "2_ops/2_num.cas",
        "2_ops/3_fs.cas",
        "2_ops/4_time.cas",
        "2_ops/6_dict.cas",
        "2_ops/7_expr.cas",
        "2_ops/8_str_v2.cas",
        "2_ops/9_list.cas",
        "2_ops/10_dict_v2.cas",
        "2_ops/test_perf.cas",
        "4_intensive/1_benchmark.cas",
        "4_intensive/2_line_count.cas"
    ]
    
    # Files that return a dict of { "check_name": true }
    checkpoint_tests = [
        "1_flow/4_copy.cas",
        "2_ops/7_expr.cas",
        "2_ops/8_str_v2.cas",
        "2_ops/9_list.cas",
        "2_ops/10_dict_v2.cas"
    ]
    
    print("\n=== RUNNING NORMAL TESTS ===")
    for rel_path in normal_tests:
        cas_file = script_dir + "/" + rel_path
        print(f"\n--- Running {rel_path} ---")
        try:
            res = caslang_mod.run(cas_file)
            # Since res is now a Dict (JSON object), we can access it
            # X::Value wrapper for Dict
            if res.get("success"):
                data = res.get('data')
                print(f"[{rel_path}] PASSED (Data: {data})")
                
                # Checkpoints verification
                if rel_path in checkpoint_tests:
                    print("--> Verifying Checkpoints...")
                    if isinstance(data, dict):
                        failed_checks = []
                        for k, v in data.items():
                            if v is not True:
                                failed_checks.append(f"{k}={v}")
                        
                        if failed_checks:
                            print(f"[CHECKPOINT FAILURE] The following checks failed: {', '.join(failed_checks)}")
                        else:
                            print("[CHECKPOINT SUCCESS] All checks passed.")
                    else:
                        print(f"[CHECKPOINT FAILURE] Returned data is not a dict: {type(data)}")

                logs = res.get("logs")
                if logs:
                    print(f"Captured Logs: {logs}")
            else:
                err = res.get("error")
                print(f"[{rel_path}] UNEXPECTED FAILURE: {err.get('message')} at line {err.get('line')}")
        except Exception as e:
            print(f"[{rel_path}] EXCEPTION: {e}")

    # 2. Error Tests (Expected Failure)
    error_tests = [
        ("5_errors/1_missing_endif.cas", "Unclosed scope: if"),
        ("5_errors/2_invalid_syntax.cas", "Invalid syntax (must start with #)"),
        ("5_errors/3_unclosed_loop.cas", "Unclosed scope: loop"),
        ("5_errors/4_runtime_err.cas", "Unknown namespace: bad_namespace")
    ]
    
    print("\n=== RUNNING ERROR TESTS ===")
    for rel_path, expected_substr in error_tests:
        cas_file = script_dir + "/" + rel_path
        print(f"\n--- Running {rel_path} (Expected Error) ---")
        try:
            res = caslang_mod.run(cas_file)
            if not res.get("success"):
                err = res.get("error")
                msg = err.get("message")
                line = err.get("line")
                if expected_substr in msg:
                    print(f"[{rel_path}] PASSED: Correctly caught error '{msg}' at line {line}")
                else:
                    print(f"[{rel_path}] FAILED: Caught wrong error '{msg}', expected '{expected_substr}'")
            else:
                print(f"[{rel_path}] FAILED: Unexpected success")
        except Exception as e:
             print(f"[{rel_path}] EXCEPTION: {e}")

    print("\nAll tests completed.")

except Exception as e:
    print(f"Failed to import caslang: {e}")
    exit(1)
