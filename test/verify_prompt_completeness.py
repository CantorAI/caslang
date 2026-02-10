import re
import os

PROMPT_FILE = r"D:\CantorAI\caslang\Prompts\CAS_Prompt.hpp"
CORE_DIR = r"D:\CantorAI\caslang\core"
CAS_LANG_CPP = os.path.join(CORE_DIR, "CasLang.cpp")

def get_registered_ops():
    """Checks CasLang.cpp to see which Ops classes are registered."""
    registered = []
    with open(CAS_LANG_CPP, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
        # Look for runner.Register(std::make_unique<Cas...Ops>());
        # Also check for commented out lines
        for line in content.splitlines():
            if "runner.Register" in line and "Cas" in line and "Ops" in line:
                # Check if commented out
                if line.strip().startswith("//"):
                    continue
                match = re.search(r"Cas(\w+)Ops", line)
                if match:
                    registered.append(match.group(1).lower()) # e.g. "Num" -> "num" (but we need namespace)
                
    # Map class name part to namespace
    # CasStringOps -> str
    # CasFSOps -> fs
    # CasNumOps -> num
    # CasTimeOps -> time
    # CasDictOps -> dict
    # CasListOps -> list
    ns_map = {
        "string": "str",
        "fs": "fs",
        "num": "num",
        "time": "time",
        "dict": "dict",
        "list": "list"
    }
    
    return [ns_map.get(r, r) for r in registered]

def parse_prompt_commands():
    """Extracts #ns.cmd from the prompt file."""
    commands = set()
    with open(PROMPT_FILE, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
        # Regex for #ns.cmd
        matches = re.findall(r"#(\w+)\.(\w+)", content)
        for ns, cmd in matches:
            commands.add(f"{ns}.{cmd}")
    return commands

def parse_cpp_commands(registered_namespaces):
    """Extracts commands from C++ files."""
    defined_commands = set()
    
    # 1. CasRunner.cpp (Flow commands)
    runner_path = os.path.join(CORE_DIR, "CasRunner.cpp")
    with open(runner_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
        # Look for 'if (cmd == "name")' or 'else if (cmd == "name")' inside flow block
        # Heuristic: just grab all 'cmd == "name"' and assume flow if typically found there
        # Better: Search specifically for flow commands known list, or specific block
        # For simplicity, let's grep for 'cmd == "..."' and filter commonly known flow cmds
        matches = re.findall(r'cmd\s*==\s*"(\w+)"', content)
        flow_cmds = ["set", "get", "if", "else", "endif", "loop_start", "loop_end", "break", "continue", "retry_start", "retry_end", "return"]
        for m in matches:
            if m in flow_cmds:
                defined_commands.add(f"flow.{m}")

    # 2. Ops files
    ops_files = {
        "str": "CasStringOps.h",
        "fs": "CasFSOps.h",
        "num": "CasNumOps.h",
        "time": "CasTimeOps.h",
        "dict": "CasDictOps.h",
        "list": "CasListOps.h"
    }
    
    for ns, filename in ops_files.items():
        if ns not in registered_namespaces and ns != "flow": # flow is always there
             print(f"Skipping {ns} (not registered in CasLang.cpp)")
             continue
             
        path = os.path.join(CORE_DIR, filename)
        if not os.path.exists(path):
            print(f"Warning: {path} not found")
            continue
            
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            # Look for 'if (command == "name")'
            matches = re.findall(r'command\s*==\s*"(\w+)"', content)
            for m in matches:
                defined_commands.add(f"{ns}.{m}")

    return defined_commands

def verify():
    print("Verifying CAS_Prompt.hpp completeness...")
    
    registered_ns = get_registered_ops()
    print(f"Registered namespaces: {registered_ns}")
    
    prompt_cmds = parse_prompt_commands()
    code_cmds = parse_cpp_commands(registered_ns)
    
    missing_in_prompt = code_cmds - prompt_cmds
    extra_in_prompt = prompt_cmds - code_cmds
    
    # Filter out some known exceptions or false positives?
    # e.g. #tool.call is in prompt but handled via external handler/CasFilter logic
    # We should add logic to whitelist tool.call if it's not found in C++ core scan
    if "tool.call" in extra_in_prompt:
        extra_in_prompt.remove("tool.call") # Handled in CasFilter/Prompt logic specifically
    
    # sandbox.exec is also special
    if "sandbox.exec" in extra_in_prompt:
        extra_in_prompt.remove("sandbox.exec")

    print("\n--- Summary ---")
    print(f"Commands in Prompt: {len(prompt_cmds)}")
    print(f"Commands in Code:   {len(code_cmds)}")
    
    if missing_in_prompt:
        print("\n[MISSING] Found in Code but NOT in Prompt:")
        for c in sorted(missing_in_prompt):
            print(f"  {c}")
    else:
        print("\n[OK] No commands missing from prompt.")

    if extra_in_prompt:
        print("\n[EXTRA] Found in Prompt but NOT in (scanned) Code:")
        for c in sorted(extra_in_prompt):
            print(f"  {c}")
    else:
        print("\n[OK] No extra commands in prompt.")
        
    code_aliases = {
        "str.match": "used for regex matching",
        "str.count_match": "used for regex counting"
    }

if __name__ == "__main__":
    verify()
