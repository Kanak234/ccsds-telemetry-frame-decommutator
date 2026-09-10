#include "ccsds/descrambler.hpp"

namespace ccsds {

// Precomputes the 255-byte repeating CCSDS PN sequence.
// Why it was written: In satellite telemetry, long sequences of static 0s or 1s
// cause RF power spectral density peaks and receiver bit synchronization loss.
// Descrambling reverses the pseudo-randomization applied at the transmitter.
// Generating the byte table once at startup allows instant in-place XOR
// processing.
void Descrambler::generate_sequence() {
  // 255 bytes = 2040 bits, exactly 8 full periods of the 255-bit PN cycle
  sequence_.resize(255);

  // Shift register initialized to all 1s (0xFF) per CCSDS 131.0-B-3
  uint8_t shift_reg = 0xFF;

  for (size_t byte_idx = 0; byte_idx < 255; ++byte_idx) {
    uint8_t byte_val = 0;
    for (int bit_idx = 7; bit_idx >= 0; --bit_idx) {
      // Output bit is the MSB of the current shift register state
      uint8_t out_bit = (shift_reg >> 7) & 1;
      byte_val |= static_cast<uint8_t>(out_bit << bit_idx);

      // Polynomial feedback: x^8 + x^7 + x^5 + x^3 + 1
      // taps at bit positions: 7, 6, 4, 2 (0-indexed from LSB to MSB)
      uint8_t feedback = ((shift_reg >> 7) ^ (shift_reg >> 6) ^
                          (shift_reg >> 4) ^ (shift_reg >> 2)) &
                         1;

      shift_reg = static_cast<uint8_t>((shift_reg << 1) | feedback);
    }
    sequence_[byte_idx] = byte_val;
  }
}

Descrambler::Descrambler() { generate_sequence(); }

void Descrambler::process(std::span<uint8_t> data) const {
  // The descrambler sequence repeats every 255 bytes.
  // XOR is self-inverting: Scrambling and Descrambling are mathematically
  // identical.
  const size_t seq_len = sequence_.size();
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] ^= sequence_[i % seq_len];
  }
}

} // namespace ccsds
