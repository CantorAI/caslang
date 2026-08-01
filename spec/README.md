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

# CasLang specifications

This directory contains normative language definitions. Words such as **MUST**,
**MUST NOT**, **SHOULD**, and **MAY** are requirements in the sense of RFC 2119.

- `CASLANG-0.3.md` defines the portable core language.
- `operations-core-0.3.md` defines the exact operation signatures.
- `instruction.schema.json` defines the common JSON instruction envelope.
- `profiles/browser.md` defines browser-only capabilities layered on the core.
- `IMPLEMENTATION-STATUS.md` records known runtime/spec conformance gaps.

The older `docs/casLang-Spec-0.3.md` is the LLM-facing generation guide. When it
conflicts with this directory, the normative files in `spec/` take precedence.

An implementation claiming `core-0.3` conformance MUST pass the core conformance
suite without requiring XLang or a product-specific host. Host operations are
available only when declared in the execution capability manifest.
