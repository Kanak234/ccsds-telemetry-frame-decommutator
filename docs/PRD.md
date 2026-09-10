# Product Requirements Document (PRD)

## Project: CCSDS Telemetry Frame Decommutator (`ccsds-telemetry-frame-decommutator`)

### 1. Executive Summary
The `ccsds-telemetry-frame-decommutator` is a high-performance, deterministic C++20 software engine designed to ingest raw spacecraft downlinked telemetry bitstreams, perform frame synchronization and error correction, descramble the data units, and decommutate CCSDS TM (Telemetry) / AOS (Advanced Orbiting Systems) Transfer Frames into discrete CCSDS Space Packets.

### 2. Problem Statement
Ground station software receivers and mission operations centers receive raw digitized RF bitstreams that suffer from bit flips, Doppler shifts, phase ambiguities, and symbol dropouts. To reliably reconstruct spacecraft telemetry, the data stream must be:
1. Synchronized against 32-bit Attached Sync Markers (ASM).
2. De-interleaved and forward-error-corrected via Reed-Solomon (255, 223) decoding.
3. Descrambled using the CCSDS pseudo-random noise generator.
4. Validated via CRC-16-CCITT integrity checks.
5. Decommutated across virtual channels into Application Process Identifiers (APIDs).

Existing solutions are either locked behind closed aerospace toolkits or embedded in heavyweight satellite command-and-control software that cannot easily be embedded into modern microservice or edge ground-station pipelines.

### 3. Target Users
- SmallSat / CubeSat ground station software operators.
- Aerospace software engineers developing satellite mission operations ground segments.
- Telecommunications researchers analyzing SDR (Software Defined Radio) bitstream recordings from NOAA, ESA, or NASA missions.

### 4. Goals and Non-Goals
#### Goals:
- Deliver deterministic, zero-allocation C++20 parsing capable of processing >500 MB/s of CADU frames.
- Full compliance with CCSDS 131.0-B-3 (TM Synchronization and Channel Coding) and CCSDS 132.0-B-2 (TM Space Data Link Protocol).
- Complete implementation of the CCSDS Frame Synchronization State Machine (SEARCH, CHECK, LOCK, FLYWHEEL).
- Complete implementation of Reed-Solomon (255, 223) error correction over $GF(2^8)$ with Berlekamp-Massey decoding and Chien search.
- Space Packet reassembly with First Header Pointer (FHP) handling across split frame boundaries.

#### Non-Goals:
- RF baseband demodulation (QPSK/BPSK demodulation is assumed to precede this pipeline).
- Uplink telecommand (TC) encoding (this is strictly a downlinked telemetry decommutator).

### 5. Functional Requirements
- **FR-1:** Synchronizer must identify 32-bit forward ASM (`0x1ACFFC1D`) and inverted ASM (`0xE53003E2`) with configurable Hamming distance tolerance ($E_{tol} \in [0, 5]$).
- **FR-2:** Synchronizer must transition through formal 4-state lifecycle: SEARCH $\rightarrow$ CHECK $\rightarrow$ LOCK $\rightarrow$ FLYWHEEL $\rightarrow$ SEARCH.
- **FR-3:** Reed-Solomon decoder must correct up to 16 byte errors per (255, 223) codeword and flag uncorrectable frames.
- **FR-4:** Pseudo-random descrambler must implement the CCSDS 8th-degree polynomial $h(x) = x^8 + x^7 + x^5 + x^3 + 1$.
- **FR-5:** Transfer frame parser must extract Master Channel ID (MCID), Virtual Channel ID (VCID), frame counts, and Operational Control Field (OCF).
- **FR-6:** Packet extractor must process First Header Pointers (FHP) to reassemble fragmented CCSDS Space Packets across multiple transfer frames.

### 6. Acceptance Criteria
- 100% pass rate on test suite covering Galois Field arithmetic, Reed-Solomon error correction, ASM search state transitions, and packet extraction.
- $\ge 80\%$ test coverage on core decoding modules.
- Verification against standard CCSDS Green Book test vectors.
- Compilation with zero warnings under `-Wall -Wextra -Werror -pedantic`.
- Execution of a local benchmark measuring CADU throughput in megabytes per second.
