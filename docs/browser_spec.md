<!--
Copyright (C) 2026 CantorAI Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# **CASLang Browser Engine Specification v1.0**
#### JS Interpreter ?Browser Automation Context

CASLang Browser Engine is a lightweight JavaScript interpreter designed to execute a specialized subset of CASLang JSONL natively inside a web browser's isolated context (e.g., via `content.js` or `chrome.scripting.executeScript`).

Primary objective: **High-performance, CSP-compliant, zero-IPC DOM orchestration.**

---

## **A) ARCHITECTURE DIFFERENCES (vs C++ Engine)**

* **Native Execution:** Runs inside the V8 engine on the webpage.
* **No I/O or System APIs:** EXCLUDES `fs.*`, `os.*`, `sandbox.*`, and `tool.call`.
* **Object References:** Because it runs in JavaScript, CASLang variables can hold **live DOM Element references** in addition to standard primitives (strings, numbers, lists, dicts).
* **Strict Variable Resolution:** If an argument is exactly `"${my_element}"`, the interpreter returns the raw JS object reference, enabling element chaining. If interpolated (e.g., `"Found: ${my_element}"`), it is cast to a string.

---

## **B) CORE SUPPORTED OPS (INHERITED)**

The Browser Engine implements the core logical operations of standard CASLang:

* **Control Flow:**
  * `flow.loop_start`, `flow.loop_end`, `flow.break`, `flow.continue`
  * `flow.if`, `flow.else`, `flow.endif` (Supports simple binary operators: `==`, `!=`, `<`, `>`, `<=`, `>=`)
  * `flow.return` (returns the final value to the C++ Daemon)
  * `flow.retry_start`, `flow.retry_end`
* **Assignment & Expressions:**
  * `flow.set` (supports `=` expression mode, block mode, and standard JSON parsing)
* **Data Structures:**
  * `list.new`, `list.append`, `list.remove`, `list.len`, `list.range`
  * `dict.new`, `dict.set`, `dict.remove`, `dict.has`, `dict.keys`
* **Utility Namespaces (Fully Compatible with C++ Spec):**
  * `str.*`: `str.contains` (args: `s`, `sub`), `str.replace`, `str.split`, `str.len`, `str.count`
  * `time.*`: `time.now`, `time.sleep`
  * `json.*`: `json.parse` (args: `s`), `json.save` (args: `obj`)

---

## **C) BROWSER OPS (NEW)**

All browser ops operate either on a CSS `selector` string, or directly on an `element` object reference (retrieved via query ops).

### **C1. DOM Queries & Traversal (jQuery style)**

```json
{"op":"browser.query", "selector":"#my-id", "as":"el"}
{"op":"browser.query_all", "selector":"tr.data-row", "as":"elements"}
{"op":"browser.visible_text_nodes", "as":"nodes", "max":300, "chunk_size":700, "store":true}
{"op":"browser.get_text_node_chunks", "scan_id":"txtscan_...", "requests":[{"node_id":"n0","offset_start":700,"offset_end":2100}], "as":"chunks"}
```
* Finds elements. `query_all` returns a list. If `element` is provided (e.g. `"element":"${parentEl}"`), queries are scoped within that parent.
* `browser.visible_text_nodes` returns visible text-node preview records in page reading order, sorted with viewport-visible nodes first. Each record includes `text`, `scan_id`, `node_id`, `offset_start`, `offset_end`, `original_length`, `has_more`, `source_hint`, `evidence`, `confidence`, parent `tag`, `role`, `aria_label`, `placeholder`, `rect`, `font_size`, `font_weight`, and `in_viewport`. With `store:true`, full node text is cached in tab session state for later chunk expansion.
* `browser.get_text_node_chunks` reads requested offsets from a stored visible text-node scan. It is used when a text node was too large for the first pass and the caller needs more chunks.

```json
{"op":"browser.parent", "element":"${el}", "as":"parentEl"}
{"op":"browser.children", "element":"${el}", "as":"childrenList"}
{"op":"browser.siblings", "element":"${el}", "as":"siblingList"}
```
* DOM traversal ops to move up, down, or across the DOM tree relative to a live element.

### **C2. Interactivity**

```json
{"op":"browser.click", "element":"${el}"}
{"op":"browser.type", "element":"${el}", "text":"Hello CantorAI"}
{"op":"browser.hover", "element":"${el}"}
{"op":"browser.focus", "element":"${el}"}
{"op":"browser.blur", "element":"${el}"}
{"op":"browser.scroll_to", "element":"${el}"}
```
* Native interactions. `browser.type` triggers input events mimicking real keyboard activity. `browser.scroll_to` ensures the element is in the viewport.

### **C3. Attributes, CSS, and Data Extraction**

```json
{"op":"browser.get_text", "element":"${el}", "as":"text"}
{"op":"browser.get_html", "element":"${el}", "as":"html"}
{"op":"browser.set_html", "element":"${el}", "html":"<b>Bold</b>"}
```
* Read/write internal content.

```json
{"op":"browser.get_attr", "element":"${el}", "attr":"href", "as":"link"}
{"op":"browser.set_attr", "element":"${el}", "attr":"href", "val":"https://..."}
{"op":"browser.remove_attr", "element":"${el}", "attr":"disabled"}
```
* Manipulate element attributes dynamically.

```json
{"op":"browser.add_class", "element":"${el}", "class":"highlight"}
{"op":"browser.remove_class", "element":"${el}", "class":"highlight"}
{"op":"browser.has_class", "element":"${el}", "class":"highlight", "as":"hasClass"}
```
* Manage CSS classes for conditional styling or tracking.

```json
{"op":"browser.get_style", "element":"${el}", "prop":"display", "as":"displayVal"}
{"op":"browser.set_style", "element":"${el}", "prop":"display", "val":"none"}
```
* Direct inline style manipulation.

### **C4. Synchronization & Waiting**

```json
{"op":"browser.wait_for", "selector":".loading", "state":"hidden", "timeout":5000}
{"op":"browser.wait_for", "selector":".result", "state":"visible", "timeout":5000}
```
* Suspends JS execution using `MutationObserver` until the condition is met.
* `state` can be `visible`, `hidden`, `attached`, or `detached`.
* Throws a runtime error (triggering `flow.retry_start` if active) on timeout.

```json
{"op":"time.sleep", "ms": 500}
```
* Hard delay (equivalent to `await new Promise(r => setTimeout(r, ms))`). Use sparingly in favor of `wait_for`.

---

## **D) UTILITY OPS (STR, TIME, JSON)**

Because CSP forbids `eval()`, complex expression logic must be broken down using explicit operations. Our operations perfectly match the C++ engine signature.

```json
{"op":"str.contains", "s":"${rowText}", "sub":"Error", "as":"hasError"}
```

```json
{"op":"time.now", "as":"currentMs"}
```

Many modern sites (Next.js, Nuxt) embed state data. `json.*` allows native extraction:
```json
{"op":"browser.get_text", "element":"${scriptTag}", "as":"rawJson"}
{"op":"json.parse", "s":"${rawJson}", "as":"stateDict"}
{"op":"flow.set", "name":"userId", "value":"${stateDict['props']['pageProps']['user']['id']}"}
```

---

## **E) EXAMPLE EXECUTION FLOW**

LLM JSONL Output (sent to Daemon, piped to Extension):

```jsonl
{"op":"list.new", "as":"results"}
{"op":"browser.query_all", "selector":"table tr.row", "as":"rows"}
{"op":"flow.loop_start", "var":"row", "in":"${rows}"}
  {"op":"browser.get_text", "element":"${row}", "as":"rowText"}
  
  // CSP-safe comparison using standard str ops
  {"op":"str.contains", "s":"${rowText}", "sub":"Error", "as":"hasError"}
  
  // flow.if handles simple binary evaluation natively
  {"op":"flow.if", "cond":"${hasError} == true"}
    
    // Scoped query within parent element
    {"op":"browser.query", "element":"${row}", "selector":".delete-btn", "as":"btn"}
    {"op":"browser.click", "element":"${btn}"}
    {"op":"time.sleep", "ms":100}
    {"op":"list.append", "list":"${results}", "value":"Deleted error row"}
    
  {"op":"flow.endif"}
{"op":"flow.loop_end"}
{"op":"flow.return", "value":"${results}"}
```

---

## **F) ERROR HANDLING (STRICT PARITY)**

The JS Interpreter must behave **identically** to the C++ Engine regarding errors. When an error occurs, execution halts (unless caught by `flow.retry_start`) and a structured error object is returned to the C++ Daemon.

* **Unsupported Operations:** If the LLM hallucinates a command not supported in the browser context (e.g., `fs.read_file`), the engine MUST instantly throw `E2001 E_OP_UNKNOWN`.
* **Missing Arguments:** If an expected argument is missing (e.g., `browser.click` without `element` or `selector`), throw `E2101 E_ARG_MISSING`.
* **Variable Not Found:** If a variable interpolation fails, throw `E2201 E_VAR_UNDEFINED`.
* **DOM Runtime Errors:** If a selector is completely invalid or an operation throws a native JS exception, it must be caught and thrown as `E3001 E_RUNTIME_EXCEPTION`.
