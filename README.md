# caslang

**caslang** is a constrained scripting language designed for expressing
**validated, executable workflow plans**.

Instead of iterating through multiple LLM tool calls, an LLM generates
a **single caslang script** that describes *what to do*.
The host system validates and executes the script deterministically
under strict capability and security constraints.

caslang follows the same idea as graph query languages:
collapse multi-step interactions into a single executable plan.

---

## Why caslang

Modern LLM systems often rely on repeated tool calls:

LLM → tool → result → LLM → tool → result …

This approach increases latency, cost, hallucination risk,
and often requires leaking local data to the model.

caslang takes a different approach:

- The model outputs **one script**
- The script is **fully validated before execution**
- Execution happens **locally**, under host-defined policies
- Only **approved results** are returned

---

## Design principles

- **Single-pass execution**
  - One script, one validation, one execution
- **Strong constraints**
  - Closed command set
  - Strict argument schemas
  - Deterministic control flow
- **Separation of concerns**
  - Language core defines *control*
  - Capabilities define *what exists*
  - Executor defines *how it runs*
- **Privacy by default**
  - Local data stays local
- **Sugar-free core**
  - Any syntax sugar must desugar into the canonical form

---

## What caslang is (and is not)

**caslang is**
- A scripting language
- A workflow / execution-plan language
- LLM-friendly and machine-validated
- Host-embedded and policy-driven

**caslang is not**
- A general-purpose programming language
- An agent framework
- A chat protocol
- A runtime that executes arbitrary model-written code

---

## Canonical form

A caslang script consists of **one command per line**:

