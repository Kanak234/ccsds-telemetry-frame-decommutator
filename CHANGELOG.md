# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-09-11

### Added
- Pure C++20 Attached Synchronization Marker (ASM) frame synchronizer with 4-state automaton (`SEARCH`, `CHECK`, `LOCK`, `FLYWHEEL`).
- Automatic $180^\circ$ BPSK phase inversion detection and real-time polarity correction.
- Galois Field $GF(2^8)$ arithmetic engine with compile-time lookup tables using CCSDS primitive polynomial $p(x) = x^8 + x^7 + x^2 + x + 1$.
- CCSDS Reed-Solomon $RS(255, 223)$ forward error correction codec with Horner syndrome calculation, Berlekamp-Massey algorithm, Chien search, and Forney evaluation.
- Support for CCSDS symbol interleaving depths ($I = 1, 2, 3, 4, 5, 8$).
- Fast table-driven CCSDS 8-bit LFSR pseudo-random descrambler ($h(x) = x^8 + x^7 + x^5 + x^3 + 1$).
- Fast table-driven CCSDS Frame Error Control Field (FECF) CRC-16-CCITT integrity verification.
- CCSDS 132.0-B-2 Telemetry Transfer Frame primary header decoder and validation engine.
- CCSDS 133.0-B-1 Space Packet Protocol extractor with cross-frame fragment reassembly via First Header Pointer (FHP) and idle packet filtering.
- Production command-line tool `ccsds_decommutator` supporting binary file ingestion, live telemetry metrics, and JSON telemetry reporting.
- High-performance throughput benchmark tool `benchmark_throughput`.
- Multi-stage Docker containerization and GitHub Actions CI workflow.
- Complete documentation suite (`docs/PRD.md`, `docs/TRD.md`, `docs/IMPLEMENTATION_PLAN.md`, `docs/EXPLAIN.md`).
