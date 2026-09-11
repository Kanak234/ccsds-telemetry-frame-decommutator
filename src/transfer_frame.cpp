#include "ccsds/transfer_frame.hpp"

namespace ccsds {

TransferFrameParser::TransferFrameParser(bool check_crc) : check_crc_(check_crc) {}

std::optional<UnpackedFrame> TransferFrameParser::parse(
    std::span<const uint8_t> frame_bytes) const {
  // Minimum Transfer Frame size: Primary Header (6 bytes) + minimum 1 byte data
  if (frame_bytes.size() < TF_PRIMARY_HEADER_SIZE + 1) {
    return std::nullopt;
  }

  UnpackedFrame result;

  // 1. Parse 6-byte Primary Header
  uint8_t b0 = frame_bytes[0];
  uint8_t b1 = frame_bytes[1];
  uint8_t b2 = frame_bytes[2];
  uint8_t b3 = frame_bytes[3];
  uint8_t b4 = frame_bytes[4];
  uint8_t b5 = frame_bytes[5];

  result.header.version = static_cast<uint8_t>((b0 >> 6) & 0x03);
  result.header.spacecraft_id = static_cast<uint16_t>(((b0 & 0x3F) << 4) | ((b1 >> 4) & 0x0F));
  result.header.virtual_channel_id = static_cast<uint8_t>((b1 >> 1) & 0x07);
  result.header.ocf_flag = (b1 & 0x01) != 0;
  result.header.master_frame_count = b2;
  result.header.virtual_frame_count = b3;
  result.header.secondary_header_flag = (b4 & 0x80) != 0;
  result.header.synch_flag = (b4 & 0x40) != 0;
  result.header.packet_order_flag = (b4 & 0x20) != 0;
  result.header.segment_length_id = static_cast<uint8_t>((b4 >> 3) & 0x03);
  result.header.first_header_pointer = static_cast<uint16_t>(((b4 & 0x07) << 8) | b5);

  // 2. Validate CRC-16 Frame Error Control Field (FECF) if enabled
  size_t trailing_overhead = 0;
  if (check_crc_) {
    trailing_overhead += 2;  // 2-byte CRC
  }
  if (result.header.ocf_flag) {
    trailing_overhead += 4;  // 4-byte OCF
  }

  if (frame_bytes.size() < TF_PRIMARY_HEADER_SIZE + trailing_overhead) {
    return std::nullopt;
  }

  if (check_crc_) {
    result.crc_valid = crc16_.verify(frame_bytes);
    if (!result.crc_valid) {
      result.valid = false;
      return result;  // CRC check failed!
    }
  } else {
    result.crc_valid = true;
  }

  // 3. Extract Optional Operational Control Field (OCF)
  size_t data_end = frame_bytes.size() - (check_crc_ ? 2 : 0);
  if (result.header.ocf_flag) {
    data_end -= 4;
    uint32_t ocf_val = (static_cast<uint32_t>(frame_bytes[data_end]) << 24) |
                       (static_cast<uint32_t>(frame_bytes[data_end + 1]) << 16) |
                       (static_cast<uint32_t>(frame_bytes[data_end + 2]) << 8) |
                       static_cast<uint32_t>(frame_bytes[data_end + 3]);
    result.ocf = ocf_val;
  }

  // 4. Extract Transfer Frame Data Field
  size_t data_start = TF_PRIMARY_HEADER_SIZE;
  if (data_end > data_start) {
    result.data_field.assign(frame_bytes.begin() + static_cast<std::ptrdiff_t>(data_start),
                             frame_bytes.begin() + static_cast<std::ptrdiff_t>(data_end));
  }

  result.valid = true;
  return result;
}

std::vector<uint8_t> TransferFrameParser::pack(const TransferFrameHeader& header,
                                               std::span<const uint8_t> payload,
                                               std::optional<uint32_t> ocf, bool append_crc) {
  std::vector<uint8_t> frame;
  size_t total_size = TF_PRIMARY_HEADER_SIZE + payload.size();
  if (ocf.has_value()) {
    total_size += 4;
  }
  if (append_crc) {
    total_size += 2;
  }
  frame.reserve(total_size);

  // Byte 0: Version (2 bits) | SCID MSB (6 bits)
  uint8_t b0 =
      static_cast<uint8_t>(((header.version & 0x03) << 6) | ((header.spacecraft_id >> 4) & 0x3F));
  // Byte 1: SCID LSB (4 bits) | VCID (3 bits) | OCF Flag (1 bit)
  uint8_t b1 = static_cast<uint8_t>(((header.spacecraft_id & 0x0F) << 4) |
                                    ((header.virtual_channel_id & 0x07) << 1) |
                                    (ocf.has_value() ? 0x01 : 0x00));
  // Byte 2: MC Frame Count
  uint8_t b2 = header.master_frame_count;
  // Byte 3: VC Frame Count
  uint8_t b3 = header.virtual_frame_count;
  // Byte 4: Sec Header (1) | Synch (1) | Pkt Order (1) | Seg Len (2) | FHP MSB
  // (3)
  uint8_t b4 = static_cast<uint8_t>(
      (header.secondary_header_flag ? 0x80 : 0x00) | (header.synch_flag ? 0x40 : 0x00) |
      (header.packet_order_flag ? 0x20 : 0x00) | ((header.segment_length_id & 0x03) << 3) |
      ((header.first_header_pointer >> 8) & 0x07));
  // Byte 5: FHP LSB (8 bits)
  uint8_t b5 = static_cast<uint8_t>(header.first_header_pointer & 0xFF);

  frame.push_back(b0);
  frame.push_back(b1);
  frame.push_back(b2);
  frame.push_back(b3);
  frame.push_back(b4);
  frame.push_back(b5);

  // Append Payload
  frame.insert(frame.end(), payload.begin(), payload.end());

  // Append OCF if present
  if (ocf.has_value()) {
    uint32_t val = ocf.value();
    frame.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    frame.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    frame.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(val & 0xFF));
  }

  // Append CRC-16-CCITT if enabled
  if (append_crc) {
    Crc16 crc;
    uint16_t crc_val = crc.compute(frame);
    frame.push_back(static_cast<uint8_t>((crc_val >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(crc_val & 0xFF));
  }

  return frame;
}

}  // namespace ccsds
