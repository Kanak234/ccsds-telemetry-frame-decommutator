# Implementation Plan — `ccsds-telemetry-frame-decommutator`

## Tasks & Milestones

- [x] **Phase 1: Foundation & Data Types**
  - [x] Implement `include/ccsds/types.hpp` with CADU, Transfer Frame, and Space Packet structures.
  - [x] Implement `include/ccsds/crc16.hpp` and `src/crc16.cpp` for CCSDS CRC-16-CCITT integrity verification.
  - [x] Implement `include/ccsds/descrambler.hpp` and `src/descrambler.cpp` for CCSDS LFSR sequence descrambling.

- [x] **Phase 2: Error Correction ($GF(2^8)$ & Reed-Solomon)**
  - [x] Implement `include/ccsds/galois_field.hpp` and `src/galois_field.cpp` with lookup tables and arithmetic operations.
  - [x] Implement `include/ccsds/reed_solomon.hpp` and `src/reed_solomon.cpp` with syndrome calculation, Berlekamp-Massey, Chien search, and Forney evaluation.

- [x] **Phase 3: Synchronization & Framing**
  - [x] Implement `include/ccsds/frame_sync.hpp` and `src/frame_sync.cpp` implementing the 4-state ASM state machine.
  - [x] Implement `include/ccsds/transfer_frame.hpp` and `src/transfer_frame.cpp` with header unpacking and FECF validation.

- [x] **Phase 4: Packet Decommutation & Pipeline Assembly**
  - [x] Implement `include/ccsds/packet_extractor.hpp` and `src/packet_extractor.cpp` handling First Header Pointer reassembly.
  - [x] Implement `include/ccsds/decommutator.hpp` and `src/decommutator.cpp` orchestrating the full end-to-end processing pipeline.
  - [x] Implement `src/main.cpp` providing the command-line interface.

- [x] **Phase 5: Verification & Benchmarking**
  - [x] Write unit test suites in `tests/` covering every component.
  - [x] Implement end-to-end telemetry file decommutation test with synthetic bit errors and packet fragmentation.
  - [x] Write `benchmarks/benchmark_throughput.cpp` measuring frames/sec and MB/s throughput on this host.
  - [x] Verify `-Wall -Wextra -Werror -pedantic` compilation and run `clang-format`.

- [x] **Phase 6: Documentation & Release Packaging**
  - [x] Write `docs/EXPLAIN.md` covering all 6 mandatory sections.
  - [x] Write `README.md` with architecture diagram, measured benchmark numbers, and example output.
  - [x] Write `Dockerfile`, `.github/workflows/ci.yml`, `LICENSE`, `CHANGELOG.md`.
  - [x] Create initial git commits and tag `v1.0.0`.
