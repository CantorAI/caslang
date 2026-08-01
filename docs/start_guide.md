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

# CasLang Start Guide

## Running CasLang Scripts Directly

You can execute `.cas` files directly using the standalone `caslang` executable.

### Command Syntax

```bash
caslang <path-to-script.cas>
```

-   **`caslang`**: The dependency-free CasLang command-line runtime.
-   **`<path_to_cas_file>`**: Absolute or relative path to your CasLang script.

### Example

Assuming `caslang` is on `PATH`:

```bash
# Run a simple echo test
caslang test/6_sandbox/1_echo.cas
```

### Supported Features

When running in this mode:
-   The **core CasLang library** fits naturally into the execution environment.
-   **Sandbox operations** (`#sandbox.exec`) are fully supported.
-   **File system operations** (`#fs.*`) are available.
-   **Output** is printed directly to stdout/stderr.

### Exit Codes

The process returns `0` on successful execution and a nonzero code for parse,
validation, runtime, or file errors.
