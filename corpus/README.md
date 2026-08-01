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

# CasLang corpus

This directory contains versioned material for training and evaluating models
that generate or repair CasLang. `corpus` is used instead of `dataset` because
the repository contains curated seeds, validation metadata, repair pairs, and
held-out evaluations from which release datasets can be built.

## Layout

```text
corpus/
  schema/record.schema.json
  seed/train.jsonl
  seed/eval.jsonl
```

Every record references a normative `spec_version` and a `profile`. Training
records MUST contain only synthetic, public, or explicitly licensed material.
Customer prompts, credentials, personal data, production traces, and proprietary
tool results MUST NOT be added.

Before release, every `generate` record MUST pass the matching parser, validator,
and capability checks. Every `repair` record MUST demonstrate that the invalid
script fails with the stated error and that the correction succeeds. Evaluation
scenarios MUST be split by scenario family rather than randomly duplicated from
training examples.
