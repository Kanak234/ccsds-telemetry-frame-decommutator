# Technical Requirements Document (TRD)

## Project: CCSDS Telemetry Frame Decommutator (`ccsds-telemetry-frame-decommutator`)

### 1. Architecture Overview
The engine operates as a unidirectional dataflow pipeline:
```
Raw Bit/Byte Stream
       │
       ▼
┌─────────────────────────┐
│ Frame Synchronizer      │  <-- Correlates 32-bit ASM with Hamming tolerance
│ (SEARCH, CHECK, LOCK,   │      Manages Flywheel state
│  FLYWHEEL)              │
└────────────┬────────────┘
             │ Synchronized CADU Frame
             ▼
┌─────────────────────────┐
│ Reed-Solomon Decoder    │  <-- Interleaving depth I=1..5
│ (GF(2^8) (255, 223))    │      Berlekamp-Massey + Chien search + Forney
└────────────┬────────────┘
             │ Error-Corrected Frame
             ▼
┌─────────────────────────┐
│ Pseudo-Random           │  <-- Linear Feedback Shift Register (LFSR)
│ Descrambler             │      Polynomial: x^8 + x^7 + x^5 + x^3 + 1
└────────────┬────────────┘
             │ Descrambled Transfer Frame
             ▼
┌─────────────────────────┐
│ Transfer Frame Parser   │  <-- Parses Primary Header (Version, SCID, VCID)
│ & CRC-16-CCITT Verifier │      Verifies Frame Error Control Field (FECF)
└────────────┬────────────┘
             │ Valid Frame Data Field + FHP
             ▼
┌─────────────────────────┐
│ Space Packet Extractor  │  <-- Reassembles split packets across frames
│ (CCSDS 133.0-B-1)       │      Emits discrete SpacePackets with APID & payload
└─────────────────────────┘
```

### 2. Technology Stack & Specifications
- **Language Standard:** C++20 (`std::span`, `std::ranges`, `std::bit_cast`, `std::array`, concepts)
- **Build System:** CMake $\ge 3.20$
- **Compiler Requirements:** GCC $\ge 11$ or Clang $\ge 14$
- **Compiler Flags:** `-Wall -Wextra -Werror -pedantic -O3 -fPIC`
- **Dependencies:** Header-only or internal algorithms (zero third-party runtime dependencies for maximum portability and deterministic execution)
- **Testing Framework:** Lightweight standalone test harness with detailed assertion reporting and coverage tracking.

### 3. Data Models & Protocols
#### 3.1 Attached Sync Marker (ASM)
- Forward ASM: `0x1ACFFC1D` (32 bits)
- Inverted ASM: `0xE53003E2` (bitwise NOT of forward ASM, occurs on 180° carrier phase ambiguity)

#### 3.2 Galois Field $GF(2^8)$
- Primitive Field Polynomial: $p(x) = x^8 + x^7 + x^2 + x + 1$ (`0x187` in binary representation)
- Generator Root: $\alpha = 0x02$
- Log and Exponential lookup tables (`exp_table`, `log_table`) computed at compile-time / initialization for $O(1)$ multiplication and inversion.

#### 3.3 Reed-Solomon (255, 223)
- Codeword length: $n = 255$ bytes
- Message length: $k = 223$ bytes
- Parity symbols: $2t = 32$ bytes ($t = 16$ symbol error correction capability)
- Generator polynomial:
  $$g(x) = \prod_{j=112}^{143} (x - \alpha^j)$$
  following standard CCSDS 131.0-B-3 dual-basis representation.

#### 3.4 Transfer Frame Primary Header (6 bytes)
- Version Number: 2 bits
- Spacecraft ID (SCID): 10 bits
- Virtual Channel ID (VCID): 3 bits
- Operational Control Field Flag: 1 bit
- Master Channel Frame Count: 8 bits
- Virtual Channel Frame Count: 8 bits
- Transfer Frame Data Field Status: 16 bits (First Header Pointer: 11 bits)

### 4. Technical Trade-Offs & Decisions
- **Decision 1:** Use precomputed log/antilog tables for Galois field arithmetic rather than on-the-fly polynomial division.
  - *Rationale:* Memory footprint is only 512 bytes (256 bytes for log, 512 bytes for exp with wraparound), but yields $>20\times$ speedup during Chien root search.
- **Decision 2:** Implement zero-copy buffer slicing via `std::span<const uint8_t>` throughout the pipeline.
  - *Rationale:* Eliminates heap allocations during real-time telemetry processing, ensuring zero memory fragmentation and deterministic sub-microsecond latency per frame.
- **Decision 3:** Standalone test framework.
  - *Rationale:* Enables instant, deterministic building and execution without external network downloads or package manager issues.
