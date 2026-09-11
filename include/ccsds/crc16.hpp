#ifndef CCSDS_CRC16_HPP
#define CCSDS_CRC16_HPP

#include <array>
#include <cstdint>
#include <span>

namespace ccsds {

/**
 * Implements the Frame Error Control Field (FECF) verification using
 * CRC-16-CCITT according to CCSDS 132.0-B-2 Section 4.1.6.
 *
 * Characteristic Polynomial: x^16 + x^12 + x^5 + 1 (0x1021)
 * Initial Value: 0xFFFF
 * Final XOR: 0x0000
 */
class Crc16 {
 public:
  Crc16();

  // Computes the 16-bit CRC over the specified data buffer.
  uint16_t compute(std::span<const uint8_t> data) const;

  // Verifies whether a frame with an appended 2-byte CRC matches.
  // The total span includes the data bytes and the trailing 2-byte CRC.
  bool verify(std::span<const uint8_t> frame_with_crc) const;

 private:
  std::array<uint16_t, 256> table_{};

  // Precomputes the 256-entry lookup table for O(1) byte-wise processing
  void init_table();
};

}  // namespace ccsds

#endif  // CCSDS_CRC16_HPP
