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

# Runtime conformance status

This file prevents implemented behavior from being mistaken for normative
behavior while Core 0.3 conformance tests are being completed.

Implemented without XLang:

- Portable `Cas::Value` domain
- JSONL parsing and raw block parsing
- Core control-flow executor
- List, dictionary, string, JSON, time, filesystem, tool, and sandbox handlers
- Standalone static library and CLI
- Browser JavaScript profile

Known gaps to close before tagging `v0.3.0`:

- Generate exhaustive per-operation JSON Schemas from the normative signature table.
- Validate every operation and argument before the first external effect.
- Add an explicit host capability-manifest object to the public C++ API.
- Normalize all error results to the structured envelope.
- Add deterministic replay fixtures for time and host responses.
- Decide whether experimental raw-block `interpolate:false` becomes Core 0.4 or
  remains a named extension profile.

Corpus release jobs MUST use only behaviors marked conforming by the test suite.
