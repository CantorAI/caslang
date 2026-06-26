import xlang
import os
import json

try:
    caslang_mod = xlang.importModule("caslang", fromPath="caslang")
    script_dir = "D:/CantorAI/caslang/test"
    
    error_tests = [
        ("5_errors/1_missing_endif.cas", "Unclosed scope: if"),
        ("5_errors/2_invalid_syntax.cas", "Invalid syntax (must start with #)"),
        ("5_errors/3_unclosed_loop.cas", "Unclosed scope: loop"),
        ("5_errors/4_runtime_err.cas", "Unknown namespace: bad_namespace")
    ]
    
    print("\n=== RUNNING ERROR TESTS ONLY ===")
    for rel_path, expected_substr in error_tests:
        cas_file = script_dir + "/" + rel_path
        print(f"\n--- Testing {rel_path} ---")
        try:
            res = caslang_mod.run(cas_file)
            if not res.get("success"):
                err = res.get("error")
                msg = err.get("message")
                line = err.get("line")
                print(f"Error Message: '{msg}'")
                print(f"Error Line: {line}")
                
                if expected_substr in msg:
                    print(f"RESULT: PASSED")
                else:
                    print(f"RESULT: FAILED (Expected '{expected_substr}')")
            else:
                print(f"RESULT: FAILED (Unexpected success)")
        except Exception as e:
             print(f"RESULT: EXCEPTION: {e}")

except Exception as e:
    print(f"Failed to import caslang: {e}")
    exit(1)
