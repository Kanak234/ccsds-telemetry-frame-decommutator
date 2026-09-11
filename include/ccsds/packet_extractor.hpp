#ifndef CCSDS_PACKET_EXTRACTOR_HPP
#define CCSDS_PACKET_EXTRACTOR_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "ccsds/types.hpp"

namespace ccsds {

/**
 * Reassembles CCSDS Space Packets (CCSDS 133.0-B-1) across Transfer Frame
 * boundaries respecting First Header Pointer (FHP) offsets and Virtual Channel
 * streams.
 */
class PacketExtractor {
 public:
  explicit PacketExtractor(bool filter_idle = true);

  // Ingests the data field of an unpacked Transfer Frame and extracts all
  // completed Space Packets.
  std::vector<SpacePacket> ingest_frame_data(uint8_t vcid, uint16_t first_header_pointer,
                                             std::span<const uint8_t> data_field);

  // Parses a single standalone Space Packet from a contiguous byte buffer
  static std::optional<SpacePacket> parse_packet(std::span<const uint8_t> packet_bytes);

  // Serializes a SpacePacket structure into standard CCSDS 6-byte header +
  // payload bytes
  static std::vector<uint8_t> serialize_packet(const SpacePacket& packet);

  // Resets reassembly buffers across all virtual channels
  void reset();

  void set_filter_idle(bool filter) noexcept { filter_idle_ = filter; }
  bool filter_idle() const noexcept { return filter_idle_; }

 private:
  bool filter_idle_{true};

  // Per-VCID reassembly state for packets spanning across frame boundaries
  struct VcReassemblyBuffer {
    std::vector<uint8_t> buffer;
  };
  std::map<uint8_t, VcReassemblyBuffer> vc_buffers_;

  // Extracts completed packets from a continuous buffer
  std::vector<SpacePacket> extract_from_buffer(std::vector<uint8_t>& buf);
};

}  // namespace ccsds

#endif  // CCSDS_PACKET_EXTRACTOR_HPP
