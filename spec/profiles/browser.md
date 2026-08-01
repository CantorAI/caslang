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

# Browser profile for CasLang Core 0.3

The browser profile adds `browser.*` DOM operations and persistent `session.*`
state to Core 0.3. It is implemented by the JavaScript runtime in `js/` and is
not required for core conformance.

A browser host MUST declare individual supported operations. DOM handles are
host values and MUST NOT be serialized into portable corpus outputs. Browser
operations may read or mutate only the document and session authorized by the
embedding browser host.

The detailed operation catalog remains in `docs/browser_spec.md` until its JSON
schemas and conformance tests are complete.
