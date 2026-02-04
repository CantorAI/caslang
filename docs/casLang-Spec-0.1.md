# CASLang v1 — Complete Specification 

This document is the **single canonical specification** for CASLang v1:

* language rules
* full runtime function set (core + standard capabilities)
* structured retry semantics with automatic backoff
* time capabilities (`time.now`, `time.sleep`)
* unified **error phases** and **stable error codes**

---

## 1. Script Output Rules (Mandatory)

1. Output **ONLY** CASLang commands
2. **Exactly one command per line**
3. No explanations, no comments, no markdown, no code fences
4. Any non-command text invalidates the script

---

## 2. Command Line Syntax

Each line MUST be:

```
#<namespace>(.<subnamespace>)*.<command>{ <json-args> }
```

---

## 3. JSON Argument Rules (Strict)

Inside `{...}`:

* Double quotes only
* No trailing commas
* Values MUST be scalars only: `string | number | boolean | null`
* Arrays/objects are NOT allowed as values

  * If needed, pass them as a **stringified JSON string** (a string that contains JSON text)

Invalid JSON MUST produce a PARSE error.

---

## 4. Variables

* Create variables via:

  * `"as":"name"` (for commands that define `as`)
  * `#flow.set{"name":"...","value":...}`
* Read variables only inside JSON **string** values using `${name}`
* Special variable: `${_last}` = previous command output
* Variable names: `[A-Za-z_][A-Za-z0-9_]*`
* Referencing an undefined variable is invalid

---

## 5. Execution Model

* Execute top-to-bottom
* `${_last}` updates after every command
* `#flow.return` ends execution immediately
* If no `#flow.return`, final result is `${_last}`

---

## 6. Condition Expressions (for `#flow.if`)

`cond` is a string expression supporting:

* `|| && !`
* `== != < <= > >=`
* parentheses

Comparison semantics:

* number vs number → numeric
* otherwise → string compare (case-sensitive)

Invalid expressions MUST be rejected.

---

# 7. Runtime Function Set

CASLang is **function-free** (no user-defined functions, no goto).
It includes:

* **Core** functions (`#flow.*`) always valid
* **Standard capabilities** (specified here; a runtime may expose a subset)

Every function below includes an example line.

---

# 7A. Core Functions (`#flow.*`)

## 7A.1 `#flow.loop_start`

**Example**

```text
#flow.loop_start{"var":"x","in":"[\"a\",\"b\",\"c\"]","index":"i","from":0,"limit":-1}
```

**Args**

* `var` (string, required): loop item variable name
* `in` (string, required): stringified JSON array OR comma/newline list string
* `index` (string, optional): index variable name (0-based)
* `from` (number, optional, default 0): start index
* `limit` (number, optional, default -1): max iterations, -1 = no limit (policy may still cap)

**Return**: `true`

---

## 7A.2 `#flow.loop_end`

**Example**

```text
#flow.loop_end{}
```

**Args**: none
**Return**: `true`

---

## 7A.3 `#flow.if`

**Example**

```text
#flow.if{"cond":"(${n} > 0) && (${ok} == true)"}
```

**Args**

* `cond` (string, required)

**Return**: `true`

---

## 7A.4 `#flow.else`

**Example**

```text
#flow.else{}
```

**Args**: none
**Return**: `true`

---

## 7A.5 `#flow.endif`

**Example**

```text
#flow.endif{}
```

**Args**: none
**Return**: `true`

---

## 7A.6 `#flow.break`

**Example**

```text
#flow.break{}
```

**Args**: none
**Return**: `true`

---

## 7A.7 `#flow.continue`

**Example**

```text
#flow.continue{}
```

**Args**: none
**Return**: `true`

---

## 7A.8 `#flow.return`

**Example**

```text
#flow.return{"value":"${result}"}
```

**Args**

* `value` (string|number|boolean|null, optional)

**Return**: `value` (or `null` if omitted)

---

## 7A.9 `#flow.set`

**Example**

```text
#flow.set{"name":"count","value":0}
```

**Args**

* `name` (string, required)
* `value` (string|number|boolean|null, required)

**Return**: `true`

---

## 7A.10 `#flow.get`

**Example**

```text
#flow.get{"name":"count","as":"c"}
```

**Args**

* `name` (string, required)
* `as` (string, required)

**Return**: the stored value

---

## 7A.11 `#flow.exists`

**Example**

```text
#flow.exists{"name":"count","as":"has_count"}
```

**Args**

* `name` (string, required)
* `as` (string, required)

**Return**: boolean

---

## 7A.12 `#flow.retry_start`  ✅ (Core)

**Example**

```text
#flow.retry_start{"times":5,"backoff_ms":200,"backoff":"exponential","max_backoff_ms":2000,"jitter_ms":50,"retry_on":"E3204,E3304,E3002","as":"retry_ctx"}
```

**Args**

* `times` (number, required): max attempts (>=1)
* `backoff_ms` (number, optional, default 0): base backoff in ms before retry
* `backoff` (string, optional, default "fixed"): `"fixed"` or `"exponential"`
* `max_backoff_ms` (number, optional, default = backoff_ms): upper bound for backoff
* `jitter_ms` (number, optional, default 0): random jitter upper bound in ms
* `retry_on` (string, optional): comma-separated error codes allowed to retry

  * if omitted, **all RUNTIME errors** are retryable
* `as` (string, optional): variable name to store retry context

**Return**: `true`

**Retry context variables (system-provided, read-only within retry block)**

* `${_attempt}` (number): current attempt number (1..times)
* `${_err_code}` (string): last runtime error code within the retry block
* `${_err_msg}` (string): last runtime error message within the retry block

If `as` is provided, `${as}` MUST be set to a stringified JSON object after each attempt:

```json
{"attempt":1,"last_code":"E3204","last_message":"..."}
```

---

## 7A.13 `#flow.retry_end` ✅ (Core)

**Example**

```text
#flow.retry_end{}
```

**Args**: none
**Return**: `true`

**Semantics (Normative)**

* The retry block is the sequence of commands between `retry_start` and `retry_end`.
* If all commands in the block succeed, execution continues after `retry_end`.
* If a command in the block fails with a **RUNTIME** error:

  * If attempts remain and the error code is retryable:

    * the runtime MUST automatically apply backoff (see Section 9)
    * then re-run the entire retry block from its first command
  * Otherwise the runtime MUST stop and return error `E3501 E_RETRY_EXHAUSTED`.

---

# 7B. Number Functions (`#num.*`) — Standard Capability

All return numeric scalars unless stated otherwise.

## 7B.1 `#num.add`

**Example**

```text
#num.add{"a":1,"b":2,"as":"sum"}
```

**Args**: `a` (number), `b` (number), `as` (string)
**Return**: `a + b`

---

## 7B.2 `#num.sub`

**Example**

```text
#num.sub{"a":10,"b":3,"as":"diff"}
```

**Args**: `a` (number), `b` (number), `as` (string)
**Return**: `a - b`

---

## 7B.3 `#num.mul`

**Example**

```text
#num.mul{"a":6,"b":7,"as":"prod"}
```

**Args**: `a` (number), `b` (number), `as` (string)
**Return**: `a * b`

---

## 7B.4 `#num.div`

**Example**

```text
#num.div{"a":10,"b":2,"as":"q"}
```

**Args**: `a` (number), `b` (number), `as` (string)
**Return**: `a / b`
**Invalid**: division by zero → `E3101 E_NUM_DIV0`

---

## 7B.5 `#num.min`

**Example**

```text
#num.min{"a":3,"b":9,"as":"m"}
```

**Args**: `a` (number), `b` (number), `as` (string)
**Return**: `min(a,b)`

---

## 7B.6 `#num.max`

**Example**

```text
#num.max{"a":3,"b":9,"as":"m"}
```

**Args**: `a` (number), `b` (number), `as` (string)
**Return**: `max(a,b)`

---

# 7C. String Functions (`#str.*`) — Standard Capability

## 7C.1 `#str.len`

**Example**

```text
#str.len{"s":"hello","as":"n"}
```

**Args**: `s` (string), `as` (string)
**Return**: number length

---

## 7C.2 `#str.trim`

**Example**

```text
#str.trim{"s":"  hi \n","as":"t"}
```

**Args**: `s` (string), `as` (string)
**Return**: string

---

## 7C.3 `#str.lower`

**Example**

```text
#str.lower{"s":"HeLLo","as":"lo"}
```

**Args**: `s` (string), `as` (string)
**Return**: string

---

## 7C.4 `#str.upper`

**Example**

```text
#str.upper{"s":"HeLLo","as":"up"}
```

**Args**: `s` (string), `as` (string)
**Return**: string

---

## 7C.5 `#str.contains`

**Example**

```text
#str.contains{"s":"hello world","needle":"world","as":"has"}
```

**Args**: `s` (string), `needle` (string), `as` (string)
**Return**: boolean

---

## 7C.6 `#str.find`

**Example**

```text
#str.find{"s":"hello world","needle":"world","as":"pos"}
```

**Args**: `s` (string), `needle` (string), `as` (string)
**Return**: number index or `-1`

---

## 7C.7 `#str.replace`

**Example**

```text
#str.replace{"s":"a-b-c","from":"-","to":"_","as":"y"}
```

**Args**: `s` (string), `from` (string), `to` (string), `as` (string)
**Return**: string (replace all occurrences)

---

## 7C.8 `#str.slice`

**Example**

```text
#str.slice{"s":"abcdef","start":1,"end":4,"as":"sub"}
```

**Args**: `s` (string), `start` (number), `end` (number), `as` (string)
**Return**: string

---

## 7C.9 `#str.match`

**Example**

```text
#str.match{"s":"abc123","regex":"[0-9]+","case":"sensitive","as":"m"}
```

**Args**

* `s` (string, required)
* `regex` (string, required)
* `case` (string, optional): `"insensitive"` or `"sensitive"` (default `"sensitive"`)
* `as` (string, required)

**Return**

* boolean `false` if no match
* OR a stringified JSON object with:

  * `ok` (boolean) = true
  * `match` (string)
  * `groups` (array) (inside returned JSON string)
  * `pos` (number)

---

## 7C.10 `#str.count_match`

**Example**

```text
#str.count_match{"s":"a1b2c3","regex":"[0-9]","case":"sensitive","as":"n"}
```

**Args**: `s` (string), `regex` (string), `case` (optional), `as` (string)
**Return**: number of non-overlapping matches

---

# 7D. File Functions (`#fs.*`) — Standard Capability

## 7D.1 `#fs.read_file`

**Example**

```text
#fs.read_file{"path":"./a.txt","offset":0,"max_bytes":1024,"as":"data"}
```

**Args**

* `path` (string, required)
* `offset` (number, optional, default 0)
* `max_bytes` (number, optional, default -1; -1 means all)
* `as` (string, required)

**Return**: string

---

## 7D.2 `#fs.write_file`

**Example**

```text
#fs.write_file{"path":"./out.txt","data":"hello\n","append":true,"as":"w"}
```

**Args**

* `path` (string, required)
* `data` (string, required)
* `append` (boolean, optional, default false)
* `as` (string, optional)

**Return**: `true`

---

## 7D.3 `#fs.list`

**Example**

```text
#fs.list{"dir":".","pattern":"*.cpp","recursive":true,"include_dirs":false,"as":"names"}
```

**Args**

* `dir` (string, required)
* `pattern` (string, optional, default `"*"`)
* `recursive` (boolean, optional, default false)
* `include_dirs` (boolean, optional, default false)
* `as` (string, required)

**Return**: stringified JSON array of strings

---

## 7D.4 `#fs.delete`

**Example**

```text
#fs.delete{"path":"./tmp.txt","recursive":false}
```

**Args**

* `path` (string, required)
* `recursive` (boolean, optional, default false)

**Return**: `true`

---

## 7D.5 `#fs.copy`

**Example**

```text
#fs.copy{"src":"./a.txt","dst":"./b.txt","overwrite":true,"recursive":false}
```

**Args**

* `src` (string, required)
* `dst` (string, required)
* `overwrite` (boolean, optional, default false)
* `recursive` (boolean, optional, default true)

**Return**: `true`

---

## 7D.6 `#fs.move`

**Example**

```text
#fs.move{"src":"./a.txt","dst":"./moved/a.txt","overwrite":true}
```

**Args**

* `src` (string, required)
* `dst` (string, required)
* `overwrite` (boolean, optional, default false)

**Return**: `true`

---

## 7D.7 `#fs.mkdir`

**Example**

```text
#fs.mkdir{"path":"./build/out","recursive":true}
```

**Args**

* `path` (string, required)
* `recursive` (boolean, optional, default true)

**Return**: `true`

---

## 7D.8 `#fs.stat`

**Example**

```text
#fs.stat{"path":"./a.txt","as":"st"}
```

**Args**

* `path` (string, required)
* `as` (string, required)

**Return**: stringified JSON object:
`{"path":"...","exists":true,"is_dir":false,"is_file":true,"size":123}`

---

## 7D.9 `#fs.exists`

**Example**

```text
#fs.exists{"path":"./a.txt","as":"ok"}
```

**Args**

* `path` (string, required)
* `as` (string, required)

**Return**: boolean

---

# 7E. Time Functions (`#time.*`) — Standard Capability ✅

## 7E.1 `#time.now`

**Example**

```text
#time.now{"as":"t_ms"}
```

**Args**

* `as` (string, required)

**Return**

* number: Unix epoch time in milliseconds

---

## 7E.2 `#time.sleep`

**Example**

```text
#time.sleep{"ms":200,"as":"ok"}
```

**Args**

* `ms` (number, required)
* `as` (string, optional)

**Return**

* `true`

---

# 7F. External Tool Invocation (`#tool.call`) — Standard Capability

## 7F.1 `#tool.call`

**Example**

```text
#tool.call{"target":"node://A","name":"edgehub.get_status","args":"{\"device_id\":\"123\"}","timeout_ms":5000,"as":"r"}
```

**Args**

* `target` (string, optional)
* `name` (string, required)
* `args` (string, required): stringified JSON object
* `timeout_ms` (number, optional, default 5000)
* `as` (string, required)

**Return**: tool-defined (string or stringified JSON string)

---

# 7G. Sandbox Script Execution (`#sandbox.exec`) — Standard Capability

## 7G.1 `#sandbox.exec`

**Example**

```text
#sandbox.exec{"lang":"python","code":"import json\nopen('${out_path}','w').write('{\"ok\":true}')\n","cwd":".","out_path":"./result.json","timeout_ms":3000,"as":"r"}
```

**Args**

* `lang` (string, required): `"python"` | `"powershell"` | `"bash"`
* `code` (string, required)
* `cwd` (string, required)
* `out_path` (string, required)
* `timeout_ms` (number, optional, default 3000)
* `as` (string, required)

**Return**: recommended stringified JSON object, otherwise `true`

---

# 8. Validation Rules (Correctness)

A script is invalid if:

* Any line is not a valid command line
* JSON args are invalid or contain array/object values
* Blocks are unbalanced (`loop_start/loop_end`, `if/else/endif`, `retry_start/retry_end`)
* Variables are referenced before definition
* Any command uses unknown or missing required fields
* Any argument has the wrong scalar type
* Any forbidden construct appears

---

# 9. Retry Backoff Semantics (Normative) ✅

If a retryable RUNTIME error occurs inside a retry block:

### 9.1 Backoff delay computation

Let `attempt` be the attempt number starting at 1.

* If `backoff` is `"fixed"`:

  * `delay = backoff_ms`
* If `backoff` is `"exponential"`:

  * `delay = backoff_ms * 2^(attempt-1)`

Then:

* `delay = min(delay, max_backoff_ms)`
* If `jitter_ms > 0`, add random jitter in `[0, jitter_ms]`
* The runtime MUST wait for `delay` milliseconds before re-running the block

### 9.2 Sleep responsibility

* The runtime MUST perform backoff waiting internally.
* Scripts SHOULD NOT implement manual waiting for retry backoff using `time.sleep`.

---

# 10. Unified Error Model (Phases + Codes) ✅

All failures MUST be reported using:

* `phase`: `"PARSE" | "VALIDATE" | "RUNTIME"`
* stable `code` (e.g., `E2101`)
* stable `name` (e.g., `E_ARG_MISSING`)
* consistent machine-readable payload

## 10.1 Standard Error Response Format

```json
{
  "ok": false,
  "phase": "VALIDATE",
  "code": "E2101",
  "name": "E_ARG_MISSING",
  "message": "fs.read_file missing required field: path",
  "line": 12,
  "col": 1,
  "op": "fs.read_file",
  "field": "path",
  "hint": "Add \"path\":\"...\"",
  "details": null,
  "trace_id": "optional"
}
```

Field rules:

* `line` is 1-based; `0` if not applicable
* `op` and `field` may be null if not applicable
* `details` is optional stringified JSON

---

## 10.2 Error Code Ranges

* `E1xxx` — PARSE errors
* `E2xxx` — VALIDATE errors
* `E3xxx` — RUNTIME errors

Codes MUST NOT change meaning once published.

---

## 10.3 PARSE Errors (E1xxx)

* `E1001` `E_SCRIPT_EMPTY`
* `E1002` `E_LINE_NOT_COMMAND`
* `E1003` `E_COMMAND_SYNTAX`
* `E1004` `E_JSON_INVALID`
* `E1005` `E_JSON_NON_SCALAR`
* `E1006` `E_JSON_QUOTES`

---

## 10.4 VALIDATE Errors (E2xxx)

### Operation / Capability

* `E2001` `E_OP_UNKNOWN`
* `E2002` `E_OP_NOT_ALLOWED`
* `E2003` `E_OP_NAMESPACE`

### Argument Schema

* `E2101` `E_ARG_MISSING`
* `E2102` `E_ARG_UNKNOWN_FIELD`
* `E2103` `E_ARG_TYPE`
* `E2104` `E_ARG_VALUE`
* `E2105` `E_ARG_RANGE`
* `E2110` `E_ARG_ENUM`
* `E2111` `E_ARG_PARSE_LIST`

### Variables

* `E2201` `E_VAR_UNDEFINED`
* `E2202` `E_VAR_NAME_INVALID`
* `E2203` `E_VAR_REFERENCE_CONTEXT`
* `E2204` `E_LAST_UNAVAILABLE`

### Blocks / Control Flow

* `E2301` `E_BLOCK_UNBALANCED`
* `E2302` `E_BLOCK_INVALID`
* `E2303` `E_BREAK_OUTSIDE_LOOP`
* `E2304` `E_CONTINUE_OUTSIDE_LOOP`
* `E2305` `E_RETURN_NOT_LAST`

### Retry blocks

* `E2310` `E_RETRY_BLOCK_UNBALANCED`
* `E2311` `E_RETRY_NEST_INVALID`

### Conditions

* `E2401` `E_COND_PARSE`
* `E2402` `E_COND_TYPE`

### Limits (pre-exec)

* `E2501` `E_LIMIT_STEPS`
* `E2502` `E_LIMIT_LOOP`
* `E2503` `E_LIMIT_IO_DECLARED`

---

## 10.5 RUNTIME Errors (E3xxx)

### General runtime

* `E3001` `E_RUNTIME_EXCEPTION`
* `E3002` `E_TIMEOUT`
* `E3003` `E_LIMIT_EXCEEDED`

### Number

* `E3101` `E_NUM_DIV0`

### File system

* `E3201` `E_FS_NOT_FOUND`
* `E3202` `E_FS_ACCESS`
* `E3203` `E_FS_PATH_DENIED`
* `E3204` `E_FS_IO`
* `E3205` `E_FS_ENCODING`

### Tool call

* `E3301` `E_TOOL_NOT_FOUND`
* `E3302` `E_TOOL_ARG_SCHEMA`
* `E3303` `E_TOOL_FAILED`
* `E3304` `E_TOOL_TIMEOUT`

### Sandbox

* `E3401` `E_SANDBOX_POLICY`
* `E3402` `E_SANDBOX_RUNTIME`
* `E3403` `E_SANDBOX_NO_OUTPUT`
* `E3404` `E_SANDBOX_OUTPUT_TOO_LARGE`
* `E3405` `E_SANDBOX_OUTPUT_INVALID`

### Retry

* `E3501` `E_RETRY_EXHAUSTED`

### Time

* `E3601` `E_TIME_SLEEP_LIMIT`

---

# 11. Hard Bans

Forbidden:

* `#for`, `#foreach`, `#while`
* Any unlisted command
* Pseudocode/free text/markdown
* Multi-line constructs outside JSON strings

---

# 12. End of Specification
