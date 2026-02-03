import xlang
import os

# Assuming caslang.dll is in the binary path or accessible
try:
    # fromPath should be the name of the dll without extension if in path, or full path
    # In build_all.bat environment it might be just "caslang"
    print("Attempting to import caslang module...")
    caslang_mod = xlang.importModule("caslang", fromPath="caslang")
    print("CasLang imported successfully:", caslang_mod)
    
    # Test file run
    print("Running test.cas...")
    # Assuming test.cas is in the same folder as this script, or pass absolute path
    # Python script is in D:\CantorAI\caslang\test\, so is test.cas
    script_dir = os.getcwd() # Run from bin, need relative path or arg
    # Since we run from D:\CantorAI\out\build\x64-Debug\bin
    # And test is in D:\CantorAI\caslang\test
    # Run all tests in subdirectories
    # Manually list tests to ensure execution if walk fails
    script_dir = "D:/CantorAI/caslang/test"
    test_files = [
        "0_general/1_hello.cas",
        "0_general/2_vars.cas",
        "1_flow/1_if.cas",
        "1_flow/2_loop.cas",
        "1_flow/3_retry.cas",
        "2_ops/1_str.cas",
        "2_ops/2_num.cas",
        "2_ops/3_fs.cas",
        "2_ops/4_time.cas"
    ]
    
    for rel_path in test_files:
        cas_file = script_dir + "/" + rel_path
        print(f"\n--- Running {rel_path} ---")
        try:
            res = caslang_mod.run(cas_file)
            print(f"[{rel_path}] Result: {res}")
        except Exception as e:
            print(f"[{rel_path}] FAILED: {e}")

    print("\nAll tests completed.")

except Exception as e:
    print(f"Failed to import caslang: {e}")
    exit(1)
