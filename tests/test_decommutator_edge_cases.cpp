#include <cassert>
#include <iostream>
#include <vector>

#include "ccsds/decommutator.hpp"

void test_callback_and_reset() {
  std::cout << "Testing Decommutator callback and reset...\n";
  ccsds::DecommutatorConfig config;
  config.frame_size = 259;  // 4 byte ASM + 255 byte RS block
  config.enable_reed_solomon = true;
  config.enable_descrambler = false;
  config.enable_crc_check = false;

  ccsds::Decommutator decomm(config);

  // Check initial stats
  auto s0 = decomm.stats();
  assert(s0.bytes_ingested == 0);
  assert(s0.packets_extracted == 0);

  size_t callback_count = 0;
  decomm.set_packet_callback(
      [&](const ccsds::SpacePacket&, const ccsds::TransferFrameHeader&) { callback_count++; });

  // Synthesize a valid 255-byte single RS block CADU
  ccsds::SpacePacket pkt;
  pkt.apid = 0x42;
  pkt.sequence_count = 1;
  pkt.sequence_flags = 3;
  pkt.payload = {0x01, 0x02, 0x03, 0x04};
  pkt.packet_data_length = static_cast<uint16_t>(pkt.payload.size() - 1);
  std::vector<uint8_t> pkt_bytes = ccsds::PacketExtractor::serialize_packet(pkt);

  std::vector<uint8_t> data_field(223 - ccsds::TF_PRIMARY_HEADER_SIZE, 0xAA);
  std::copy(pkt_bytes.begin(), pkt_bytes.end(), data_field.begin());

  ccsds::TransferFrameHeader tf_hdr;
  tf_hdr.virtual_channel_id = 0;
  tf_hdr.master_frame_count = 0;
  tf_hdr.first_header_pointer = 0;

  std::vector<uint8_t> tf_info =
      ccsds::TransferFrameParser::pack(tf_hdr, data_field, std::nullopt, false);
  assert(tf_info.size() == 223);

  ccsds::ReedSolomon rs(112);
  std::vector<uint8_t> codeword(255);
  rs.encode(tf_info, codeword);

  std::vector<uint8_t> cadu = {0x1A, 0xCF, 0xFC, 0x1D};
  cadu.insert(cadu.end(), codeword.begin(), codeword.end());

  // Ingest stream containing 4 consecutive CADUs to achieve lock
  std::vector<uint8_t> stream;
  for (int i = 0; i < 4; ++i) {
    stream.insert(stream.end(), cadu.begin(), cadu.end());
  }

  auto pkts = decomm.ingest(stream);
  assert(!pkts.empty());
  assert(callback_count > 0);

  // Test reset
  decomm.reset();
  auto s1 = decomm.stats();
  assert(s1.packets_extracted == 0);
  assert(s1.crc_passed_frames == 0);
}

void test_uncorrectable_and_crc_failures() {
  std::cout << "Testing RS uncorrectable and CRC error branches...\n";
  ccsds::DecommutatorConfig config;
  config.frame_size = 259;
  config.enable_reed_solomon = true;
  config.enable_descrambler = false;
  config.enable_crc_check = true;

  ccsds::Decommutator decomm(config);

  // Synthesize a frame
  std::vector<uint8_t> tf_info(223, 0x00);
  ccsds::ReedSolomon rs(112);
  std::vector<uint8_t> codeword(255);
  rs.encode(tf_info, codeword);

  // Severely corrupt codeword (30 byte errors, exceeds RS 16-error capability)
  for (size_t i = 0; i < 30; ++i) {
    codeword[i * 4] ^= 0xFF;
  }

  std::vector<uint8_t> bad_cadu = {0x1A, 0xCF, 0xFC, 0x1D};
  bad_cadu.insert(bad_cadu.end(), codeword.begin(), codeword.end());

  std::vector<uint8_t> bad_stream;
  for (int i = 0; i < 4; ++i) {
    bad_stream.insert(bad_stream.end(), bad_cadu.begin(), bad_cadu.end());
  }

  auto pkts = decomm.ingest(bad_stream);
  assert(pkts.empty());
  auto stats = decomm.stats();
  assert(stats.rs_uncorrectable_frames > 0);

  // Test CRC failure branch (RS disabled, valid ASM, corrupted CRC)
  ccsds::DecommutatorConfig crc_config;
  crc_config.frame_size = 200;
  crc_config.enable_reed_solomon = false;
  crc_config.enable_descrambler = false;
  crc_config.enable_crc_check = true;

  ccsds::Decommutator crc_decomm(crc_config);
  std::vector<uint8_t> crc_cadu = {0x1A, 0xCF, 0xFC, 0x1D};
  crc_cadu.resize(200, 0xEE);  // Random bytes with invalid CRC
  std::vector<uint8_t> crc_stream;
  for (int i = 0; i < 4; ++i) {
    crc_stream.insert(crc_stream.end(), crc_cadu.begin(), crc_cadu.end());
  }
  auto crc_pkts = crc_decomm.ingest(crc_stream);
  assert(crc_pkts.empty());
  auto crc_stats = crc_decomm.stats();
  assert(crc_stats.crc_failed_frames > 0);
}

int main() {
  std::cout << "[TEST] Running Decommutator Edge Cases Suite...\n";
  test_callback_and_reset();
  test_uncorrectable_and_crc_failures();
  std::cout << "[PASS] All Decommutator edge cases passed!\n";
  return 0;
}
