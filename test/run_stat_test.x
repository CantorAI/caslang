from caslang import caslang
import xlang_os as os

print("Running stat verification script...")
# Assume test_stat.cas is in same folder as previous test
var script_path = "D:/CantorAI/caslang/test/test_stat.cas"
var res = caslang.run(script_path)
print("Result:", res)
