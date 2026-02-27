
---

# CASLang: A Formally Verified Intermediate Representation for LLM-Driven Distributed Execution

## Abstract

Large Language Models (LLMs) are increasingly deployed as control planes for orchestrating distributed systems, heterogeneous compute clusters, and external services. However, LLM-generated execution plans are fragile: they frequently exhibit structural inconsistencies, uncontrolled side effects, and nondeterministic behavior due to external tool interactions. Existing agent frameworks rely on loosely structured JSON schemas or general-purpose scripting languages, offering limited formal guarantees and weak reproducibility.

We present **CASLang**, a restricted, formally specified intermediate representation (IR) for LLM-driven execution. CASLang provides:

1. A small-step operational semantics with oracle-based external modeling.
2. A static well-formedness and effect system ensuring structural and policy safety.
3. A deterministic replay guarantee under fixed external traces.
4. A semantics-preserving compilation from sequential scripts to parallel DAGs.

We implement CASLang atop the CantorAI distributed runtime and evaluate it on representative LLM-driven workloads. CASLang eliminates all structural runtime failures across N generated scripts, enforces policy constraints with zero violations, and achieves up to **Y% p95 latency reduction** via safe DAG parallelization.

---

## 1. Introduction

LLMs are increasingly used as high-level planners that generate execution plans invoking tools, querying databases, coordinating distributed workers, and managing heterogeneous resources (e.g., GPUs and edge nodes). These plans must ultimately execute deterministically and safely within a runtime substrate.

However, treating LLM outputs as directly executable programs leads to systemic issues:

1. **Structural Fragility.** Plans may contain malformed JSON, mismatched control blocks, or inconsistent variable references.
2. **Uncontrolled Effects.** Plans may invoke unauthorized tools, violate filesystem policies, or exceed resource constraints.
3. **Nondeterministic Behavior.** External tool calls and system interactions prevent reliable replay and debugging.

These issues arise from a fundamental mismatch between probabilistic plan generation and deterministic distributed execution.

### Key Insight

We treat LLM output not as executable code, but as programs in a **restricted, formally defined intermediate representation**. CASLang constrains expressiveness to enable static verification and semantic guarantees, while supporting:

* Structured control flow
* Tool invocation
* Filesystem operations
* Distributed task spawning
* Safe parallel execution via DAG compilation

By providing a verifiable execution layer, CASLang bridges probabilistic plan generation and deterministic system behavior.

---

## 2. Language Overview

CASLang is a JSONL-based IR with a finite instruction set. Each instruction is a JSON object containing an `op` field and structured arguments.

Example:

```json
{"op":"tool.call","tool_name":"seek_by_text","input_text":"person in white shirt","top_k":10,"as":"r"}
{"op":"flow.return","to":"final","value":"${r}"}
```

Instruction categories include:

* Pure operations (`flow.set`, `list.append`, `dict.set`)
* Control flow (`flow.if`, `flow.loop_start`)
* External operations (`tool.call`, `sandbox.exec`)
* Filesystem operations (`fs.read_file`, `fs.write_file`)
* Distributed execution (`dist.spawn`, `dist.wait`)

All instructions belong to a statically defined finite set.

---

## 3. Formal Model

### 3.1 Syntax

Let a program be a finite sequence of instructions:

[
P ::= I_1 ; I_2 ; \dots ; I_n
]

Each instruction ( I ) is drawn from a finite instruction set:

[
I ::= \textsf{Set}(x,e)
\mid \textsf{If}(e,P,P)
\mid \textsf{Loop}(x,e,P)
\mid \textsf{Return}(e)
\mid \textsf{ToolCall}(f,a,x)
\mid \textsf{FsRead}(p,x)
\mid \dots
]

JSONL serves only as serialization; the formal object is an abstract syntax tree.

---

### 3.2 State

Execution is defined over configurations:

[
\langle P, pc, \Sigma, T \rangle
]

where:

* ( P ) is the program,
* ( pc \in \mathbb{N} ) is the program counter,
* ( \Sigma = (Env, Ctrl, Out) ),
* ( T ) is an external trace.

The environment is:

[
Env : Var \rightarrow Value
]

The external trace models nondeterministic interactions:

[
T ::= e_1 \cdot e_2 \cdot \dots
]

[
e ::= \textsf{ToolResp}(f,a,r)
\mid \textsf{FsResp}(p,c)
\mid \textsf{TimeResp}(t)
]

---

### 3.3 Operational Semantics

We define a small-step transition relation:

[
\langle P, pc, \Sigma, T \rangle
\rightarrow
\langle P, pc', \Sigma', T' \rangle
]

Example rule (assignment):

[
\frac{P[pc] = \textsf{Set}(x,e)}
{\langle P, pc, \Sigma, T \rangle
\rightarrow
\langle P, pc+1, \Sigma[x \mapsto \llbracket e \rrbracket_\Sigma], T \rangle}
]

External tool call:

[
\frac{T = \textsf{ToolResp}(f,a,r) \cdot T'}
{\langle P, pc, \Sigma, T \rangle
\rightarrow
\langle P, pc+1, \Sigma[x \mapsto r], T' \rangle}
]

This oracle-based modeling isolates external nondeterminism.

---

## 4. Static Verification

### 4.1 Well-Formedness

We define a predicate ( WF(P) ) ensuring:

* Valid instruction schema
* Proper control-flow matching
* Valid variable references
* Proper block delimitation

---

### Theorem 1 (Structural Soundness)

If ( WF(P) ) holds, then execution of ( P ) cannot reach a structural failure state.

Formally, for any ( \Sigma_0 ) and ( T ):

[
\langle P, 0, \Sigma_0, T \rangle \rightarrow^*
\langle P, pc, \Sigma, T' \rangle
]

implies that either:

* execution terminates normally, or
* the next step is defined by the operational semantics.

No structural error occurs.

---

## 5. Effect System and Policy Safety

Each instruction is annotated with an effect:

[
\epsilon ::= Pure \mid IORead \mid IOWrite \mid External \mid Sandbox
]

Define:

[
Eff(P) = \bigcup eff(I_i)
]

Let ( \Pi ) be an allowed effect set.

---

### Theorem 2 (Policy Soundness)

If ( WF(P) ) and ( Eff(P) \subseteq \Pi ), then execution of ( P ) produces no effect outside ( \Pi ).

---

## 6. Deterministic Replay

We define deterministic execution relative to fixed trace ( T ).

---

### Theorem 3 (Replay Determinism)

For any well-formed program ( P ) and initial state ( \Sigma_0 ):

If

[
\langle P, \Sigma_0, T \rangle \rightarrow^* \Sigma_1
\quad\text{and}\quad
\langle P, \Sigma_0, T \rangle \rightarrow^* \Sigma_2
]

then

[
\Sigma_1 = \Sigma_2
]

Execution is deterministic under fixed trace.

---

## 7. Semantics-Preserving Parallelization

We compile sequential programs into DAGs via:

* Data dependency analysis
* Effect conflict analysis
* Control-region boundaries

Two instructions are independent if:

1. They do not share data dependencies.
2. Their effects do not conflict.

---

### Theorem 4 (Sequential Equivalence)

If the derived DAG is acyclic and independent instructions are reordered only when safe, then:

[
Exec_{DAG}(P,\Sigma_0,T)
========================

Exec_{Seq}(P,\Sigma_0,T)
]

Parallel execution preserves sequential semantics.

---

## 8. Implementation and Evaluation

We implement CASLang atop the CantorAI runtime, which provides distributed scheduling and resource-based placement.

We evaluate CASLang on:

* Tool orchestration pipelines
* Distributed database retrieval tasks
* Multi-node inference coordination

Across N generated scripts:

* Structural runtime failures reduced from **X% to 0%**
* Policy violations reduced from **X to 0**
* p95 latency improved by **Y%** via DAG parallelization
* 100% deterministic replay coverage under fixed traces

Full evaluation appears in §X.

