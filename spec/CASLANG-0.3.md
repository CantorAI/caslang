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

# CasLang Core 0.3 — Normative Specification

Status: canonical portable core specification.

## 1. Scope

CasLang is a constrained JSONL instruction language for validated workflow
execution. Core 0.3 defines syntax, values, validation, control flow, variable
resolution, host calls, results, and errors. It does not require XLang, a portal,
network transport, browser DOM, model provider, or operating-system shell.

## 2. Program representation

A program is a UTF-8 byte sequence divided into lines by `LF` or `CRLF`.
Except for raw block bodies defined in section 7, every nonempty line MUST be one
complete JSON object. Blank lines MAY be ignored. JSON comments, trailing commas,
`NaN`, and `Infinity` are invalid.

The first executable instruction MUST be:

```json
{"op":"caslang","version":"0.3"}
```

Every instruction MUST contain an `op` string. Except for the header, `op` has
the lexical form `<namespace>.<command>`, where both components match
`[a-z][a-z0-9_]*`. Unknown operations MUST fail validation before execution.

## 3. Values

The portable value domain is:

```text
null | boolean | signed 64-bit integer | finite double | UTF-8 string
     | ordered list<Value> | string-keyed dictionary<Value> | byte sequence
```

Implementations MUST preserve integer values that fit in signed 64 bits. They
MUST reject non-finite numeric JSON values. Dictionary key iteration order is
not semantically significant unless an operation explicitly sorts its output.

## 4. Program phases

Execution consists of four ordered phases:

1. **Parse:** decode JSONL and raw blocks.
2. **Validate:** check header, operation names, required arguments, argument
   types, balanced scopes, block nonces, and declared capabilities.
3. **Execute:** evaluate instructions in program order, subject to control flow.
4. **Return:** produce the result envelope from section 12.

No instruction may perform an external effect before the whole program passes
structural validation. A host MAY additionally deny an effect at runtime because
of policy or changing external state.

## 5. Variables and resolution

Variable names MUST match `[A-Za-z_][A-Za-z0-9_]*`. Implementations MUST provide
the reserved read-only variable `_last`, containing the most recent operation
result. Programs MUST NOT assign names beginning with `_`.

`${name}` resolves a variable. `${dict[key]}` resolves a dictionary key and
`${list[index]}` resolves a zero-based list index. Dot-property access is not
part of Core 0.3. A missing variable, key, or index is an error; an implementation
MUST NOT silently replace it with `null` or an empty string.

When an argument consists solely of one variable expression, the resolved value
retains its type. When a variable expression occurs inside a larger string, its
canonical string representation is substituted.

## 6. Core control flow

The following operations are structural:

```text
flow.set
flow.if / flow.else / flow.endif
flow.loop_start / flow.loop_end
flow.break / flow.continue
flow.retry_start / flow.retry_end
flow.return
```

Scopes MUST be properly nested. `flow.break` and `flow.continue` are valid only
inside a loop. `flow.else` is valid at most once for its matching `flow.if`.
`flow.return` terminates execution successfully and returns its resolved value.

Expression mode is selected when the `value` of `flow.set` is a string beginning
with `=`. Core expressions may contain literals, variable references, parentheses,
comparison operators, boolean operators, and the documented arithmetic operators.
Expressions MUST NOT invoke host tools or produce external effects.

## 7. Raw block assignment

A raw block begins with:

```json
{"op":"flow.set","name":"payload","mode":"block","nonce":"a1b2c3d4"}
```

It ends with an object whose operation is `flow.end_set` and whose `name` and
`nonce` exactly match the opener. Lines between them are raw UTF-8 text and are
not parsed as JSON. A missing or mismatched terminator is a validation error.

Core 0.3 performs normal `${...}` interpolation when the stored value is later
used. The experimental `interpolate:false` argument is not Core 0.3 and MUST be
declared by an extension profile before use.

## 8. Pure operation namespaces

Portable implementations MUST provide the operation signatures in
`operations-core-0.3.md` for these namespaces:

```text
str.*  list.*  dict.*  json.*  time.*
```

Pure operations MUST NOT access external state. Mutating list and dictionary
operations mutate only the referenced in-memory value. Invalid types or indexes
MUST return an `E2xxx` validation error when statically knowable, otherwise an
`E3xxx` runtime error.

## 9. Host capability operations

The namespaces `fs`, `tool`, and `sandbox` are host capabilities. Before
execution, the host supplies a capability manifest containing allowed operation
names and policy constraints. A program referring to an undeclared capability
MUST fail validation.

`tool.call` uses `name` to select a host tool. Reserved CasLang fields are not
forwarded. Other top-level fields are forwarded as tool arguments after variable
resolution. Tool execution MUST have a finite timeout and MUST support
cancellation. Hosts MUST report external failures as structured runtime errors.

`sandbox.exec` is optional and disabled unless explicitly declared. A conforming
host MUST apply its own process, filesystem, environment, network, and time
limits; CasLang does not imply unrestricted shell authority.

## 10. Determinism

Given the same program, initial variables, capability manifest, and recorded host
responses, a Core 0.3 execution MUST produce the same instruction trace, result,
and error. Wall-clock values, random values, filesystem state, and tool responses
are external inputs and MUST be captured when deterministic replay is requested.

## 11. Errors

Errors have a stable code, phase, message, and one-based source line:

```json
{"code":"E2101","phase":"validate","message":"missing argument: name","line":3}
```

Code families are:

```text
E1xxx parse
E2xxx validate
E3xxx runtime or host execution
```

Core header errors are `E1001 E_SCRIPT_EMPTY`, `E1006 E_HEADER_REQUIRED`, and
`E1007 E_VERSION_UNSUPPORTED`.

The first fatal error terminates the program unless it occurs inside an active
retry scope that permits another attempt. Implementations MAY attach diagnostic
details but MUST NOT change the meaning of the stable fields.

## 12. Result envelope

Success:

```json
{"success":true,"data":null,"logs":[]}
```

Failure:

```json
{"success":false,"error":{"code":"E2101","phase":"validate","message":"...","line":3},"logs":[]}
```

`logs` is ordered by emission. Hosts MAY omit logs only when the embedding API
explicitly requests it. Process-based implementations MUST return exit code `0`
for success and nonzero for failure.

## 13. Profiles and versioning

Profiles may add namespaces or optional arguments but MUST NOT change Core 0.3
semantics. Browser DOM and persistent browser-session operations belong to the
browser profile. A backward-compatible clarification increments the patch
version; new syntax or changed semantics requires a new minor version.
