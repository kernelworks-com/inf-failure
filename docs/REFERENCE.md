# Technical reference

Start with the [quick start](../README.md) to run the examples.

## Reference trace contract

The whitespace-delimited format starts with
`IFF VERSION COMPLETE DROPPED ENVIRONMENT EVENT_COUNT`. Version 1 supports only
`cpu-reference-v1`. Each event line contains
`ID KIND ALLOCATION GENERATION OPERATION STATE DEPENDENCY_COUNT [DEPENDENCIES...]`.
Identifiers and counters are unsigned decimal; state/signature names are nonempty
whitespace-free strings. Supported kinds are `allocate`, `schedule`, `retire`,
`reuse`, `consume`, and `assertion`. Assertions use zero allocation, generation,
and operation fields, with the signature in STATE. Other event kinds require
nonzero allocation/generation; schedule/consume also require a unique operation.

Events execute in file order. Dependencies must point to earlier, unique events;
this fixture format does not infer ordering across processes. Allocation generation
advances exactly once on reuse. Retirement deliberately permits an outstanding
read, and reuse models the planted allocator bug. Consumers retain the scheduled
identity, so a later physical generation produces a `stale-generation` witness.
Several readers may legally share one immutable state. The first observed failure
sets the signature; malformed lifecycle events anywhere make the trial invalid.
Missing reads at the end, incomplete capture, or dropped records are inconclusive.
The full-model semantic descriptor and engine environment manifest are deferred.

## Relationship to existing work

Failure reduction and concurrency replay are established fields. Relevant work
includes [LEAN](https://2012.splashcon.org/details/splash-2012-OOPSLA-Research-Papers/48/LEAN-simplifying-concurrency-bug-reproduction-via-replay-supported-execution-reducti),
[DEMi](https://github.com/NetSys/demi),
[GRIEF](https://arxiv.org/abs/2605.11202), and the
[PyTorch AOTInductor minifier](https://docs.pytorch.org/docs/main/user_guide/torch_compiler/torch.compiler_aot_inductor_minifier.html).
The project does not claim global novelty. Its proposed inference-specific
mechanisms require implementation and comparison with prior approaches.
