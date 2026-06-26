import xlang
import json
import os
import sys

# Always run from the project root so xlang can find modules
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
os.chdir(PROJECT_ROOT)

# Global counters
g_passed = 0
g_failed = 0
g_total = 0

# Load caslang module once
cas = xlang.importModule("caslang", fromPath="caslang")

def run_cas(file_path):
    """Run a .cas file via caslang module and return parsed result dict."""
    abs_path = os.path.abspath(os.path.join(SCRIPT_DIR, file_path)).replace("\\", "/")
    try:
        res = cas.run(abs_path)
        if isinstance(res, str):
            try:
                return json.loads(res)
            except:
                return {"raw": res}
        elif isinstance(res, dict):
            return res
        else:
            return {"raw": str(res)}
    except Exception as e:
        return {"success": False, "error": {"message": str(e)}}


def test_normal(file_path, description, check_fn=None):
    """Run a test and check for success."""
    global g_passed, g_failed, g_total
    g_total += 1

    result = run_cas(file_path)
    success_flag = result.get("success", False) if isinstance(result, dict) else False

    status = "PASS" if success_flag else "FAIL"

    if check_fn and isinstance(result, dict):
        if not check_fn(result):
            status = "FAIL"

    print(f"  [{status}] {description}: {file_path}")
    if status == "FAIL":
        print(f"    Result: {str(result)[:500]}")
        g_failed += 1
    else:
        g_passed += 1
    return status == "PASS"


def test_error(file_path, description, expected_error_substr=None):
    """Run a test that SHOULD fail, and optionally check error message."""
    global g_passed, g_failed, g_total
    g_total += 1

    result = run_cas(file_path)
    success_flag = result.get("success", True) if isinstance(result, dict) else True

    if not success_flag:
        status = "PASS"
        if expected_error_substr and isinstance(result, dict):
            err = result.get("error", {})
            err_msg = err.get("message", "") if isinstance(err, dict) else str(err)
            if expected_error_substr not in err_msg:
                status = "FAIL"
                print(f"    Expected error containing '{expected_error_substr}' but got: {err_msg}")
    else:
        status = "FAIL"

    print(f"  [{status}] {description}: {file_path}")
    if status == "FAIL":
        print(f"    Result: {str(result)[:500]}")
        g_failed += 1
    else:
        g_passed += 1
    return status == "PASS"


if __name__ == "__main__":
    print(f"\n{'='*60}")
    print(f"CasLang v0.3 JSONL Test Suite")
    print(f"{'='*60}")
    print(f"Test dir: {SCRIPT_DIR}\n")

    # === General Tests ===
    print("--- General ---")
    test_normal("0_general/1_hello.cas", "Hello World")
    test_normal("0_general/2_vars.cas", "Variable + Expression")
    test_normal("0_general/3_block_set.cas", "Block Set Mode")

    # === Flow Control ===
    print("\n--- Flow Control ---")
    test_normal("1_flow/1_if.cas", "If/Else")
    test_normal("1_flow/2_loop.cas", "Loop")
    test_normal("1_flow/3_retry.cas", "Retry")
    test_normal("1_flow/4_copy.cas", "Deep Copy")
    test_normal("1_flow/5_early_return.cas", "Early Return", lambda r: r.get("data") == 30)

    # === Operations ===
    print("\n--- Operations ---")
    test_normal("2_ops/1_str.cas", "String Upper")
    test_normal("2_ops/2_num.cas", "Expression (was num.add)")
    test_normal("2_ops/3_fs.cas", "File System")
    test_normal("2_ops/3_fs_search.cas", "FS Search (user script)")
    test_normal("2_ops/4_time.cas", "Time")
    test_normal("2_ops/6_dict.cas", "Dict stat + bracket access")
    test_normal("2_ops/7_expr.cas", "Expressions")
    test_normal("2_ops/8_str_v2.cas", "String Count")
    test_normal("2_ops/9_list.cas", "List Ops")
    test_normal("2_ops/10_dict_v2.cas", "Dict Ops")
    test_normal("2_ops/11_block_expand.cas", "Block-expand + boolean cond")
    test_normal("2_ops/12_json_ops.cas", "JSON parse + save")
    test_normal("2_ops/13_json_query.cas", "JSON query dot-path")

    # === Intensive ===
    print("\n--- Intensive ---")
    test_normal("4_intensive/1_benchmark.cas", "Benchmark")
    # test_normal("4_intensive/3_json_html_gen.cas", "JSON HTML gen (advanced)")  # needs D:\Test\2026CV\qwen_desc

    # === Error Tests ===
    print("\n--- Error Cases ---")
    test_error("5_errors/1_missing_endif.cas", "Missing Endif", "Unclosed scope: if")
    test_error("5_errors/2_invalid_syntax.cas", "Invalid Syntax", "E1004 E_JSON_INVALID")
    test_error("5_errors/3_unclosed_loop.cas", "Unclosed Loop", "Unclosed scope: loop")
    test_error("5_errors/4_runtime_err.cas", "Unknown Namespace", "E2001 E_OP_UNKNOWN")

    # === Root-level Tests ===
    print("\n--- Root-level ---")
    test_normal("test.cas", "Basic test")
    test_normal("test_continue.cas", "Continue flow")
    test_normal("test_sandbox.cas", "Sandbox exec")
    test_normal("6_sandbox/5_block_python.cas", "Sandbox block+python")
    test_error("6_sandbox/6_python_error.cas", "Sandbox python error", "command failed")
    test_normal("6_sandbox/7_heredoc.cas", "Sandbox heredoc")
    test_normal("test_stat.cas", "File stat")
    test_normal("comprehensive_v2.cas", "Comprehensive v2")

    # === Summary ===
    print(f"\n{'='*60}")
    print(f"Results: {g_passed}/{g_total} passed, {g_failed} failed")
    print(f"{'='*60}")

    sys.exit(0 if g_failed == 0 else 1)
