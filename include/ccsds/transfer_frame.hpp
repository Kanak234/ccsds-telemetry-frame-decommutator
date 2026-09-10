#ifndef CCSDS_TRANSFER_FRAME_HPP
#define CCSDS_TRANSFER_FRAME_HPP

#include "ccsds/crc16.hpp"
#include "ccsds/types.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ccsds {

/**
 * Result of unpacking and validating a CCSDS Transfer Frame.
 */
struct UnpackedFrame {
  bool valid{false};
  TransferFrameHeader header;
  std::vector<uint8_t> data_field; // Payload carrying Space Packets
  std::optional<uint32_t> ocf;     // Optional 32-bit Operational Control Field
  bool crc_valid{false};           // True if FECF CRC check passed
};

/**
 * Parses and validates CCSDS Telemetry (TM) Transfer Frames according to CCSDS
 * 132.0-B-2.
 */
class TransferFrameParser {
public:
  explicit TransferFrameParser(bool check_crc = true);

  // Parses a raw synchronized Transfer Frame buffer (without the 4-byte ASM)
  std::optional<UnpackedFrame>
  parse(std::span<const uint8_t> frame_bytes) const;

  // Packs a Transfer Frame into bytes for synthetic telemetry generation and
  // testing
  static std::vector<uint8_t> pack(const TransferFrameHeader &header,
                                   std::span<const uint8_t> payload,
                                   std::optional<uint32_t> ocf = std::nullopt,
                                   bool append_crc = true);

private:
  bool check_crc_{true};
  Crc16 crc16_;
};

} // namespace ccsds

#endif // CCSDS_TRANSFER_FRAME_HPP
