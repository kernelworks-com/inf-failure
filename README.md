# inf-failure

**Replay a recorded failure and check whether the same failure signature returns.**

inf-failure is a local C++ tool for developing reproducible inference failure
cases. It distinguishes a matching failure from a different failure, invalid
input, and incomplete evidence—so an unrelated crash does not count as a successful
reproduction.

**Available today:** an experimental CPU reference replayer with synthetic cache
lifetime fixtures. Automatic reduction and real serving-engine adapters are future
work. This version does not replay arbitrary production logs.

## Try a failing case

You need a C++20 compiler and CMake 3.20+. No GPU or model download is required.
See [build help](docs/BUILDING.md) if your tools are missing or compilation fails.

```sh
git clone https://github.com/kernelworks-com/inf-failure.git
cd inf-failure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
./build/inf-failure replay fixtures/stale.iff
```

Already in the repository? Start at the `cmake` command. Expected output:

```text
SAME_FAILURE signature=stale-generation witness=1,2,3,4,5, detail=observed reference invariant violation
```

In this fixture, a reader expects the original state, but its allocation has been
retired and reused before the read finishes. The witness lists the event IDs that
show that sequence. `SAME_FAILURE` exits with code **0**: the requested failure was
successfully reproduced.

Compare it with safe sharing:

```sh
./build/inf-failure replay fixtures/benign.iff
```

```text
NO_MATCH signature= witness= detail=reference replay completed
```

This command exits with code **1** because the target failure was absent. That is
the expected result for this fixture, not a tool crash.

## Use the CLI

```sh
# Replay a file; the default target is stale-generation.
./build/inf-failure replay fixtures/stale.iff

# Check for a specific signature.
./build/inf-failure replay fixtures/stale.iff scheduler-assert

# Write a fresh synthetic fixture to a new file.
./build/inf-failure fixture stale build/my-case.iff
./build/inf-failure replay build/my-case.iff
```

The fixture command accepts `stale` or `benign` and refuses to overwrite an existing
file. Each replay starts from fresh reference state.

| Verdict | Meaning | Exit code |
|---|---|---|
| `SAME_FAILURE` | The observed signature matches the target. | 0 |
| `NO_MATCH` | Replay completed without that failure. | 1 |
| `DIFFERENT_FAILURE` | A different failure was observed first. | 2 |
| `INVALID` | Input, prerequisites, or environment are unsupported or malformed. | 3 |
| `INCONCLUSIVE` | Recording or completion evidence is missing. | 4 |

A matching signature is evidence of reproduction, not proof of an identical root
cause. The first observed failure determines the signature; invalid lifecycle
input anywhere invalidates the trial.

## Create a reference case

Start with the [included fixtures](fixtures) and the [trace-format reference](docs/REFERENCE.md).
Events describe allocation, read scheduling, retirement, reuse, consumption, and
explicit assertions. Dependencies must point to earlier events. Incomplete
recordings remain inconclusive.

The [C++ API](include/inf_failure/replay.hpp) supports embedding the same validator
and replayer. Current cases use the `cpu-reference-v1` environment; a trace from a
real engine needs an adapter that has not been implemented yet.

The next planned capabilities are automatic case reduction, a trial ledger, and
reproducer bundles, followed by a validated engine integration. There is no GPU
support or performance claim in the current prototype.

## Tests and feedback

```sh
ctest --test-dir build --output-on-failure --no-tests=error
```

Tests cover failure matching, invalid dependencies, generation reuse, missing
evidence, and exported fixtures replayed in a fresh process. See
[CI details](.github/CI.md) for build checks and source delivery.

[Report a problem](https://github.com/kernelworks-com/inf-failure/issues) with a
synthetic case, the command you ran, and the expected verdict. Keep customer
captures and model artifacts private.

inf-failure is an independent [Kernelworks](https://github.com/kernelworks-com)
project; no sibling project is required. Licensed under [Apache-2.0](LICENSE).
