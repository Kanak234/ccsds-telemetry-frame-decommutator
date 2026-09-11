#include <cassert>
#include <iostream>

#include "ccsds/transfer_frame.hpp"

void test_transfer_frame_pack_parse() {
  ccsds::TransferFrameHeader header;
  header.version = 0;
  header.spacecraft_id = 0x2A5;  // 677
  header.virtual_channel_id = 3;
  header.master_frame_count = 42;
  header.virtual_frame_count = 17;
  header.first_header_pointer = 10;
  header.secondary_header_flag = false;
  header.synch_flag = false;

  std::vector<uint8_t> payload = {10, 20, 30, 40, 50, 60, 70, 80};
  uint32_t ocf = 0xC001CAFE;

  std::vector<uint8_t> packed =
      ccsds::TransferFrameParser::pack(header, payload, ocf, true  // with CRC
      );

  ccsds::TransferFrameParser parser(true);
  auto unpacked_opt = parser.parse(packed);

  assert(unpacked_opt.has_value());
  const auto& unpacked = unpacked_opt.value();
  assert(unpacked.valid);
  assert(unpacked.crc_valid);
  assert(unpacked.header.version == 0);
  assert(unpacked.header.spacecraft_id == 0x2A5);
  assert(unpacked.header.virtual_channel_id == 3);
  assert(unpacked.header.master_frame_count == 42);
  assert(unpacked.header.virtual_frame_count == 17);
  assert(unpacked.header.first_header_pointer == 10);
  assert(unpacked.ocf.has_value());
  assert(unpacked.ocf.value() == 0xC001CAFE);
  assert(unpacked.data_field == payload);
}

void test_corrupted_transfer_frame() {
  ccsds::TransferFrameHeader header;
  header.spacecraft_id = 100;
  header.virtual_channel_id = 1;
  header.first_header_pointer = 0;

  std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
  std::vector<uint8_t> packed =
      ccsds::TransferFrameParser::pack(header, payload, std::nullopt, true);

  // Corrupt one byte of the header
  packed[0] ^= 0xFF;

  ccsds::TransferFrameParser parser(true);
  auto unpacked_opt = parser.parse(packed);

  assert(unpacked_opt.has_value());
  // CRC verification must fail!
  assert(!unpacked_opt->valid);
  assert(!unpacked_opt->crc_valid);
}

int main() {
  std::cout << "[TEST] Running Transfer Frame Parser tests...\n";
  test_transfer_frame_pack_parse();
  test_corrupted_transfer_frame();
  std::cout << "[PASS] Transfer Frame Parser tests passed successfully.\n";
  return 0;
}
