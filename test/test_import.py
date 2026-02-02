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
    # We should hardcode for verify or use args
    cas_file = "D:/CantorAI/caslang/test/test.cas"
    print(f"Loading file: {cas_file}")
    res = caslang_mod.run(cas_file)
    print("Run result:", res)
    
    # Test string run
    print("Running direct string...")
    res_str = caslang_mod.runs('#flow.set{"name":"s","value":"direct run"}\n#flow.return{"value":"${s}"}')
    print("Runs result:", res_str)

except Exception as e:
    print(f"Failed to import caslang: {e}")
    exit(1)
