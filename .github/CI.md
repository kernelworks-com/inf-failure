# Continuous checks and delivery

The workflow runs on pull requests, pushes to `main`, and manual dispatch.

| Runner | Compiler | Configuration |
|---|---|---|
| Ubuntu 24.04 | GCC 13 | Release, warnings as errors |
| Ubuntu 24.04 | Clang 18 | Debug, address and undefined-behavior sanitizers |
| macOS 14 | Apple Clang | Release, warnings as errors |

All jobs build the portable CPU implementation and run CTest. No GPU or serving
engine is tested. Actions are pinned to immutable revisions; repository access is
read-only and checkout credentials are not persisted. Tests must exist to pass.

After every test job succeeds on `main`, source delivery creates a Git archive,
extracts it into a clean directory, builds and tests the extracted source, and
uploads `source.tar.gz` with `SHA256SUMS`. Artifacts expire after 14 days. This is
continuous delivery of a tested source bundle, not an installed service or a
published package release. Pull requests do not run the delivery job.

The workflow becomes active when pushed to a GitHub repository with Actions
enabled. Configure branch protection to require the three test jobs before
merging. That repository setting is separate from this workflow file.

To reproduce a job locally with the appropriate compiler installed:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-13 \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror" -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --no-tests=error
```

For the sanitizer job, use `clang++-18`, `Debug`, and append
`-fsanitize=address,undefined -fno-omit-frame-pointer` to the compiler flags.
