# Contributing to ccsds-telemetry-frame-decommutator

Thank you for contributing!

## Development Workflow

1. Create a dedicated branch off `master` (e.g. `feat/feature-name` or `prod-hardening`).
2. Format all C++ code using `clang-format`:
   ```bash
   clang-format -i include/ccsds/*.hpp src/*.cpp tests/*.cpp benchmarks/*.cpp
   ```
3. Verify builds, tests, and code coverage:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
   cmake --build build -j$(nproc)
   ctest --test-dir build --output-on-failure
   ```
4. Verify code coverage remains >=80% across all source files in `src/`.
5. Submit a pull request against `master`.
