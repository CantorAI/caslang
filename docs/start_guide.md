# CasLang Start Guide

## Running CasLang Scripts Directly

You can execute `.cas` files directly using the `xlang` executable without needing a separate Python runner.

### Command Syntax

```bash
xlang -caslang <path_to_cas_file>
```

-   **`xlang`**: The main executable (typically located in `bin` or your build output directory).
-   **`-caslang`**: The flag indicating direct CasLang execution.
-   **`<path_to_cas_file>`**: Absolute or relative path to your CasLang script.

### Example

Assuming you are in the directory containing `xlang.exe`:

```bash
# Run a simple echo test
xlang -caslang d:\CantorAI\caslang\test\6_sandbox\1_echo.cas
```

### Supported Features

When running in this mode:
-   The **core CasLang library** fits naturally into the execution environment.
-   **Sandbox operations** (`#sandbox.exec`) are fully supported.
-   **File system operations** (`#fs.*`) are available.
-   **Output** is printed directly to stdout/stderr.

### Exit Codes

The `xlang` process will return the exit code from the last executed command or `0` on success.
