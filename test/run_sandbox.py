import sys
import xlang

print("Importing CasLang...")
try:
    cas = xlang.importModule("caslang", fromPath="caslang")
except Exception as e:
    print(f"Failed to load caslang module: {e}")
    sys.exit(1)

test_file = r"D:\CantorAI\caslang\test\test_sandbox.cas"
print(f"Running {test_file}...")
res = cas.run(test_file)

print("\nExecution Result:")
print(res)

if isinstance(res, dict) and res.get("success"):
    print("\nSUCCESS: Script executed successfully.")
else:
    print("\nFAILURE: Script execution failed.")
