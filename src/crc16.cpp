#include "ccsds/crc16.hpp"

namespace ccsds {

// Precomputes the lookup table for the generator polynomial 0x1021
// Why it was written: Doing bit-by-bit division for every telemetry byte in
// real time introduces significant CPU overhead. A 256-entry table allows
// processing 8 bits per cycle.
void Crc16::init_table() {
  constexpr uint16_t polynomial = 0x1021;
  for (uint32_t i = 0; i < 256; ++i) {
    uint16_t curr = static_cast<uint16_t>(i << 8);
    for (int j = 0; j < 8; ++j) {
      if ((curr & 0x8000) != 0) {
        curr = static_cast<uint16_t>((curr << 1) ^ polynomial);
      } else {
        curr = static_cast<uint16_t>(curr << 1);
      }
    }
    table_[i] = curr;
  }
}

Crc16::Crc16() { init_table(); }

uint16_t Crc16::compute(std::span<const uint8_t> data) const {
  // Standard CCSDS initial seed is 0xFFFF
  uint16_t crc = 0xFFFF;
  for (uint8_t byte : data) {
    uint8_t table_index = static_cast<uint8_t>((crc >> 8) ^ byte);
    crc = static_cast<uint16_t>((crc << 8) ^ table_[table_index]);
  }
  return crc;
}

bool Crc16::verify(std::span<const uint8_t> frame_with_crc) const {
  // Check minimum length: Must have at least 1 byte of payload + 2 bytes of CRC
  if (frame_with_crc.size() < 3) {
    return false;
  }

  size_t data_len = frame_with_crc.size() - 2;
  std::span<const uint8_t> payload = frame_with_crc.subspan(0, data_len);

  // Extract the big-endian 16-bit CRC appended to the frame
  uint16_t expected_crc = static_cast<uint16_t>(
      (static_cast<uint16_t>(frame_with_crc[data_len]) << 8) |
      static_cast<uint16_t>(frame_with_crc[data_len + 1]));

  // Compute CRC on payload and compare
  uint16_t calculated_crc = compute(payload);
  return calculated_crc == expected_crc;
}

} // namespace ccsds
