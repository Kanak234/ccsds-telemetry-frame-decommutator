#include "ccsds/packet_extractor.hpp"
#include <cassert>
#include <iostream>

void test_packet_serialization_parse() {
  ccsds::SpacePacket pkt;
  pkt.version = 0;
  pkt.type = false; // Telemetry
  pkt.secondary_header_flag = false;
  pkt.apid = 0x42A;       // 1066
  pkt.sequence_flags = 3; // Unsegmented standalone
  pkt.sequence_count = 1234;
  pkt.payload = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
  pkt.packet_data_length = static_cast<uint16_t>(pkt.payload.size() - 1);

  std::vector<uint8_t> raw = ccsds::PacketExtractor::serialize_packet(pkt);
  assert(raw.size() == 6 + pkt.payload.size());

  auto parsed_opt = ccsds::PacketExtractor::parse_packet(raw);
  assert(parsed_opt.has_value());
  const auto &parsed = parsed_opt.value();

  assert(parsed.version == pkt.version);
  assert(parsed.type == pkt.type);
  assert(parsed.apid == pkt.apid);
  assert(parsed.sequence_flags == pkt.sequence_flags);
  assert(parsed.sequence_count == pkt.sequence_count);
  assert(parsed.payload == pkt.payload);
}

void test_packet_spanning_two_frames() {
  ccsds::PacketExtractor extractor;

  // Create a 20-byte payload Space Packet (6 header + 20 payload = 26 bytes
  // total)
  ccsds::SpacePacket pkt;
  pkt.apid = 0x100;
  pkt.sequence_count = 1;
  pkt.sequence_flags = 3;
  pkt.payload = std::vector<uint8_t>(20, 0x77);

  std::vector<uint8_t> raw_pkt = ccsds::PacketExtractor::serialize_packet(pkt);
  assert(raw_pkt.size() == 26);

  // Frame 1 carries the first 16 bytes of the packet (FHP = 0, packet starts at
  // offset 0)
  std::vector<uint8_t> frame1_data(raw_pkt.begin(), raw_pkt.begin() + 16);
  auto pkts1 = extractor.ingest_frame_data(1, 0, frame1_data);
  // Packet is incomplete, so no completed packets should be emitted from frame
  // 1
  assert(pkts1.empty());

  // Frame 2 carries the remaining 10 bytes at the start, followed by FHP = 10
  // Then another 10-byte packet starts at offset 10
  ccsds::SpacePacket pkt2;
  pkt2.apid = 0x200;
  pkt2.sequence_count = 2;
  pkt2.sequence_flags = 3;
  pkt2.payload = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> raw_pkt2 =
      ccsds::PacketExtractor::serialize_packet(pkt2); // 6+4 = 10 bytes

  std::vector<uint8_t> frame2_data(raw_pkt.begin() + 16,
                                   raw_pkt.end()); // 10 bytes
  frame2_data.insert(frame2_data.end(), raw_pkt2.begin(),
                     raw_pkt2.end()); // 10 bytes

  auto pkts2 = extractor.ingest_frame_data(1, 10, frame2_data);
  // Frame 2 should complete both pkt1 and pkt2!
  assert(pkts2.size() == 2);
  assert(pkts2[0].apid == 0x100);
  assert(pkts2[0].payload == pkt.payload);
  assert(pkts2[1].apid == 0x200);
  assert(pkts2[1].payload == pkt2.payload);
}

int main() {
  std::cout << "[TEST] Running Space Packet Extractor tests...\n";
  test_packet_serialization_parse();
  test_packet_spanning_two_frames();
  std::cout << "[PASS] Space Packet Extractor tests passed successfully.\n";
  return 0;
}
