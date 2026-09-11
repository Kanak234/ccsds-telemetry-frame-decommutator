#include "ccsds/packet_extractor.hpp"

namespace ccsds {

PacketExtractor::PacketExtractor(bool filter_idle) : filter_idle_(filter_idle) {}

void PacketExtractor::reset() { vc_buffers_.clear(); }

std::optional<SpacePacket> PacketExtractor::parse_packet(std::span<const uint8_t> packet_bytes) {
  if (packet_bytes.size() < SPACE_PACKET_HEADER_SIZE) {
    return std::nullopt;
  }

  uint16_t id_word = static_cast<uint16_t>((packet_bytes[0] << 8) | packet_bytes[1]);
  uint16_t seq_word = static_cast<uint16_t>((packet_bytes[2] << 8) | packet_bytes[3]);
  uint16_t length_word = static_cast<uint16_t>((packet_bytes[4] << 8) | packet_bytes[5]);

  size_t payload_len = static_cast<size_t>(length_word) + 1;
  size_t total_packet_len = SPACE_PACKET_HEADER_SIZE + payload_len;

  if (packet_bytes.size() < total_packet_len) {
    return std::nullopt;  // Incomplete packet
  }

  SpacePacket pkt;
  pkt.version = static_cast<uint8_t>((id_word >> 13) & 0x07);
  pkt.type = (id_word & 0x1000) != 0;
  pkt.secondary_header_flag = (id_word & 0x0800) != 0;
  pkt.apid = static_cast<uint16_t>(id_word & 0x07FF);

  pkt.sequence_flags = static_cast<uint8_t>((seq_word >> 14) & 0x03);
  pkt.sequence_count = static_cast<uint16_t>(seq_word & 0x3FFF);
  pkt.packet_data_length = length_word;

  pkt.payload.assign(packet_bytes.begin() + static_cast<std::ptrdiff_t>(SPACE_PACKET_HEADER_SIZE),
                     packet_bytes.begin() + static_cast<std::ptrdiff_t>(total_packet_len));

  return pkt;
}

std::vector<uint8_t> PacketExtractor::serialize_packet(const SpacePacket& packet) {
  std::vector<uint8_t> out;
  size_t payload_len = packet.payload.size();
  uint16_t len_field = payload_len > 0 ? static_cast<uint16_t>(payload_len - 1) : 0;
  out.reserve(SPACE_PACKET_HEADER_SIZE + payload_len);

  uint16_t id_word = static_cast<uint16_t>(
      ((packet.version & 0x07) << 13) | (packet.type ? 0x1000 : 0x0000) |
      (packet.secondary_header_flag ? 0x0800 : 0x0000) | (packet.apid & 0x07FF));

  uint16_t seq_word = static_cast<uint16_t>(((packet.sequence_flags & 0x03) << 14) |
                                            (packet.sequence_count & 0x3FFF));

  out.push_back(static_cast<uint8_t>((id_word >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>(id_word & 0xFF));
  out.push_back(static_cast<uint8_t>((seq_word >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>(seq_word & 0xFF));
  out.push_back(static_cast<uint8_t>((len_field >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>(len_field & 0xFF));

  out.insert(out.end(), packet.payload.begin(), packet.payload.end());
  return out;
}

std::vector<SpacePacket> PacketExtractor::extract_from_buffer(std::vector<uint8_t>& buf) {
  std::vector<SpacePacket> completed;

  while (buf.size() >= SPACE_PACKET_HEADER_SIZE) {
    uint16_t length_word = static_cast<uint16_t>((buf[4] << 8) | buf[5]);
    size_t total_packet_len = SPACE_PACKET_HEADER_SIZE + static_cast<size_t>(length_word) + 1;

    if (buf.size() < total_packet_len) {
      break;
    }

    std::span<const uint8_t> span_view(buf.data(), total_packet_len);
    auto opt_pkt = parse_packet(span_view);
    if (opt_pkt.has_value()) {
      if (!filter_idle_ || opt_pkt->apid != IDLE_APID) {
        completed.push_back(std::move(opt_pkt.value()));
      }
      buf.erase(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(total_packet_len));
    } else {
      buf.erase(buf.begin());
    }
  }

  return completed;
}

std::vector<SpacePacket> PacketExtractor::ingest_frame_data(uint8_t vcid,
                                                            uint16_t first_header_pointer,
                                                            std::span<const uint8_t> data_field) {
  std::vector<SpacePacket> packets;

  if (first_header_pointer == 0x7FE) {
    return packets;
  }

  auto& vcb = vc_buffers_[vcid];

  if (first_header_pointer == 0x7FF) {
    if (!vcb.buffer.empty()) {
      // GCC 13 emits a false-positive -Wstringop-overflow warning on
      // vector::insert from std::span iterators under -O3 optimization (GCC
      // Bugzilla #109849).
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 13)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
      vcb.buffer.insert(vcb.buffer.end(), data_field.begin(), data_field.end());
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 13)
#pragma GCC diagnostic pop
#endif
      std::vector<SpacePacket> extracted = extract_from_buffer(vcb.buffer);
      packets.insert(packets.end(), extracted.begin(), extracted.end());
    }
    return packets;
  }

  if (first_header_pointer <= data_field.size()) {
    if (first_header_pointer > 0 && !vcb.buffer.empty()) {
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 13)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
      vcb.buffer.insert(vcb.buffer.end(), data_field.begin(),
                        data_field.begin() + static_cast<std::ptrdiff_t>(first_header_pointer));
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 13)
#pragma GCC diagnostic pop
#endif
      std::vector<SpacePacket> extracted = extract_from_buffer(vcb.buffer);
      packets.insert(packets.end(), extracted.begin(), extracted.end());
    }

    vcb.buffer.clear();
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 13)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
    vcb.buffer.insert(vcb.buffer.end(),
                      data_field.begin() + static_cast<std::ptrdiff_t>(first_header_pointer),
                      data_field.end());
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 13)
#pragma GCC diagnostic pop
#endif
    std::vector<SpacePacket> extracted = extract_from_buffer(vcb.buffer);
    packets.insert(packets.end(), extracted.begin(), extracted.end());
  } else {
    vcb.buffer.clear();
  }

  return packets;
}

}  // namespace ccsds
