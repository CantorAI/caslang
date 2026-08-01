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

# CasLang Core 0.3 operation signatures

This file is normative. `T?` means an optional argument. Unless noted otherwise,
an operation may include optional `as:string`; when present, the operation result
is stored under that variable name and also becomes `_last`.

## Structural operations

| Operation | Required arguments | Optional arguments | Result |
|---|---|---|---|
| `caslang` | `version:"0.3"` | none | none |
| `flow.set` | `name:string`, `value:Value` | none | assigned value |
| `flow.set` block | `name:string`, `mode:"block"`, `nonce:string` | none in core | assigned string |
| `flow.end_set` | matching `name`, `nonce` | none | none |
| `flow.if` | `cond:string` | none | none |
| `flow.else` | none | none | none |
| `flow.endif` | none | none | none |
| `flow.loop_start` | `var:string`, `in:list` | `index:string`, `from:int=0`, `limit:int=-1` | none |
| `flow.loop_end` | none | none | none |
| `flow.break` | none | none | none |
| `flow.continue` | none | none | none |
| `flow.retry_start` | `times:int` | `backoff_ms`, `backoff`, `max_backoff_ms`, `jitter_ms`, `retry_on`, `as` | none |
| `flow.retry_end` | none | none | none |
| `flow.return` | none | `value:Value` | terminates with value |

## List and dictionary operations

Lists and dictionaries are created with `flow.set` using JSON arrays or objects.
Core 0.3 does not define `list.new` or `dict.new`.

| Operation | Required arguments | Optional arguments | Result |
|---|---|---|---|
| `list.append` | `list:list`, `value:Value` | `as` | boolean |
| `list.remove` | `list:list`, `index:int` | `as` | boolean |
| `list.len` | `list:list` | `as` | integer |
| `list.range` | `to:int` | `from:int=0`, `step:int=1`, `as` | list of integers; `to` excluded |
| `dict.get` | `dict:dict`, `key:string` | `as` | value or null |
| `dict.set` | `dict:dict`, `key:string`, `value:Value` | `as` | boolean |
| `dict.has` | `dict:dict`, `key:string` | `as` | boolean |
| `dict.remove` | `dict:dict`, `key:string` | `as` | boolean |
| `dict.keys` | `dict:dict` | `as` | list of strings; order unspecified |

`list.range` with `step:0` MUST fail validation. Negative indexes are invalid.

## String operations

| Operation | Required arguments | Optional arguments | Result |
|---|---|---|---|
| `str.print` | `msg:string` | none | boolean |
| `str.log` | `msg:string` | none | boolean |
| `str.len` | `s:string` | `as` | integer |
| `str.upper` | `s:string` | `as` | string |
| `str.lower` | `s:string` | `as` | string |
| `str.trim` | `s:string` | `as` | string |
| `str.contains` | `s:string`, `sub:string` | `as` | boolean |
| `str.find` | `s:string`, `sub:string` | `as` | integer, `-1` when absent |
| `str.replace` | `s:string`, `old:string`, `new:string` | `as` | string |
| `str.slice` | `s:string`, `start:int`, `end:int` | `as` | string, half-open range |
| `str.count` | `s:string`, `sub:string` | `as` | integer |
| `str.match` | `s:string`, `regex:string` | `case`, `as` | match record or false |
| `str.count_match` | `s:string`, `regex:string` | `case`, `as` | integer |

## JSON and time operations

| Operation | Required arguments | Optional arguments | Result |
|---|---|---|---|
| `json.parse` | `s:string` | `as` | portable value |
| `json.save` | `obj:list|dict` | `as` | compact JSON string |
| `json.query` | `obj:list|dict`, `path:string` | `as` | selected value |
| `time.now` | none | `as` | implementation timestamp |
| `time.sleep` | `ms:int` | `as` | boolean |

## Host capability operations

| Operation | Required arguments | Optional arguments | Result |
|---|---|---|---|
| `fs.read_file` | `path:string` | `offset`, `max_bytes`, `as` | string or bytes |
| `fs.write_file` | `path:string`, `data:string|bytes` | `as` | boolean |
| `fs.list` | `dir:string` | `pattern`, `recursive`, `as` | list |
| `fs.delete` | `path:string` | `recursive`, `as` | boolean |
| `fs.copy` | `src:string`, `dst:string` | `as` | boolean |
| `fs.move` | `src:string`, `dst:string` | `as` | boolean |
| `fs.mkdir` | `path:string` | `recursive`, `as` | boolean |
| `fs.stat` | `path:string` | `as` | dictionary |
| `fs.exists` | `path:string` | `as` | boolean |
| `tool.call` | `name:string` | tool fields, `timeout_ms`, `as` | host value |
| `sandbox.exec` | `command:string` | host-defined limits, `as` | host result |

The host capability manifest determines whether these operations are available
and may impose stricter path, process, network, size, and timeout constraints.
