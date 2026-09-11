#ifndef CCSDS_DESCRAMBLER_HPP
#define CCSDS_DESCRAMBLER_HPP

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace ccsds {

/**
 * Implements the standard CCSDS Pseudo-Random Descrambler according to
 * CCSDS 131.0-B-3 Section 10.
 *
 * Characteristic Generator Polynomial: h(x) = x^8 + x^7 + x^5 + x^3 + 1
 * Initial Shift Register State: 11111111 (0xFF)
 * Sequence length: 255 bytes (repeating)
 */
class Descrambler {
 public:
  Descrambler();

  // Applies in-place descrambling (or scrambling, since XOR is self-inverting)
  // starting from the first byte following the 4-byte ASM.
  void process(std::span<uint8_t> data) const;

  // Returns a copy of the pre-generated 255-byte PN sequence for verification
  const std::vector<uint8_t>& sequence() const { return sequence_; }

 private:
  std::vector<uint8_t> sequence_;

  // Precomputes the repeating 255-byte pseudo-random sequence
  void generate_sequence();
};

}  // namespace ccsds

#endif  // CCSDS_DESCRAMBLER_HPP
