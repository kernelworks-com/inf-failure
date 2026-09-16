# inf-failure

**Executed failure reproducers for inference systems, from Kernelworks.**

inf-failure is a planned tool for shrinking a failing inference request sequence
into a smaller scenario that still reproduces an identified failure in a pinned
environment. Its intended output is a runnable reproducer with replay evidence.

## Status

**Pre-implementation.** This repository contains project documentation only.
There is no runnable CLI, supported engine adapter, trained model, or published
benchmark result yet. Capabilities below are design goals.

## The problem

A serving failure can depend on earlier requests, cancellation, batching, cache
reuse, and asynchronous execution. Removing an apparently unrelated request can
remove the conditions needed to reproduce the failure. Replaying prompts alone
may not restore those conditions.

## Intended workflow

1. Record or import a request sequence and identify a specific failure signature.
2. Reproduce it in a pinned environment and measure its frequency.
3. Reduce requests and payloads while respecting known dependencies.
4. Where supported, observe engine state and preserve relevant ordering conditions.
5. Export the smaller sequence, environment manifest, replay instructions, and
   measured outcomes from fresh validation runs.

A result should state how often the same signature reproduced, what was removed,
and which instrumentation or hardware is required. An inconclusive experiment
must remain inconclusive.

## Scope

Initial work will target a CPU reference harness followed by one serving-engine
adapter and a small set of explicit failure checks. The research focus is reducing
executions involving logical cache identity, physical memory reuse, and CPU/GPU
ordering. Ordinary logs may not contain enough information to reconstruct these
conditions; additional recording may be necessary.

The tool will not infer that all timeouts share a cause, treat every malformed
model response as an engine bug, or claim that a reduced case is globally minimal.
No universal determinism or cross-hardware reproduction is promised.

## Local execution

The intended runner executes in the user's environment without mandatory hosted
inference or telemetry. Publishing captured traffic is not part of reduction.
Exported cases require review for private content and third-party permissions.

## Relationship to existing work

Failure reduction and concurrency replay are established fields. Relevant work
includes [LEAN](https://2012.splashcon.org/details/splash-2012-OOPSLA-Research-Papers/48/LEAN-simplifying-concurrency-bug-reproduction-via-replay-supported-execution-reducti),
[DEMi](https://github.com/NetSys/demi),
[GRIEF](https://arxiv.org/abs/2605.11202), and the
[PyTorch AOTInductor minifier](https://docs.pytorch.org/docs/main/user_guide/torch_compiler/torch.compiler_aot_inductor_minifier.html).
The project does not claim global novelty. Its proposed inference-specific
mechanisms require implementation and comparison with prior approaches.

## Kernelworks

inf-failure is an independent Kernelworks project. Stateguard may supply optional
state-lifetime diagnostics; inf-failure owns reduction, replay experiments, and
reproducer packaging. Yieldpoint and Branchforge are not required dependencies.

## License

Apache License 2.0. See [LICENSE](LICENSE). Third-party models, traces, and
other artifacts retain their own license terms.
