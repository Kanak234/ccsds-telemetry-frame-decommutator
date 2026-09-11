#include <cassert>
#include <iostream>
#include <vector>

#include "ccsds/decommutator.hpp"

int main() {
  std::cout << "[TEST] Running End-to-End CCSDS Telemetry Decommutation test...\n";

  ccsds::DecommutatorConfig config;
  config.frame_size = 1024;
  config.asm_tolerance = 2;
  config.enable_descrambler = true;
  config.enable_reed_solomon = true;
  config.enable_crc_check = true;

  ccsds::Decommutator decommutator(config);

  // 1. Synthesize Space Packets
  std::vector<ccsds::SpacePacket> original_packets;
  std::vector<size_t> packet_start_offsets;
  std::vector<uint8_t> packet_stream;

  for (size_t i = 0; i < 15; ++i) {
    ccsds::SpacePacket pkt;
    pkt.apid = static_cast<uint16_t>(0x100 + (i % 3));
    pkt.sequence_count = static_cast<uint16_t>(i);
    pkt.sequence_flags = 3;
    pkt.payload.resize(40 + (i * 10));
    for (size_t j = 0; j < pkt.payload.size(); ++j) {
      pkt.payload[j] = static_cast<uint8_t>((i * 17 + j) & 0xFF);
    }
    pkt.packet_data_length = static_cast<uint16_t>(pkt.payload.size() - 1);
    original_packets.push_back(pkt);

    packet_start_offsets.push_back(packet_stream.size());
    std::vector<uint8_t> s = ccsds::PacketExtractor::serialize_packet(pkt);
    packet_stream.insert(packet_stream.end(), s.begin(), s.end());
  }

  constexpr size_t RS_DEPTH = 4;
  constexpr size_t TF_INFO_LEN = RS_DEPTH * 223;                                      // 892 bytes
  constexpr size_t DATA_FIELD_LEN = TF_INFO_LEN - ccsds::TF_PRIMARY_HEADER_SIZE - 2;  // 884 bytes

  // Pad stream with a standard CCSDS Idle Packet (APID 0x7FF) to cleanly fill
  // the final frame
  size_t remainder = packet_stream.size() % DATA_FIELD_LEN;
  if (remainder != 0) {
    size_t pad_needed = DATA_FIELD_LEN - remainder;
    if (pad_needed >= ccsds::SPACE_PACKET_HEADER_SIZE) {
      ccsds::SpacePacket idle_pkt;
      idle_pkt.apid = ccsds::IDLE_APID;
      idle_pkt.sequence_flags = 3;
      idle_pkt.payload.resize(pad_needed - ccsds::SPACE_PACKET_HEADER_SIZE);
      idle_pkt.packet_data_length = static_cast<uint16_t>(idle_pkt.payload.size() - 1);
      std::vector<uint8_t> idle_raw = ccsds::PacketExtractor::serialize_packet(idle_pkt);
      packet_stream.insert(packet_stream.end(), idle_raw.begin(), idle_raw.end());
    }
  }

  ccsds::ReedSolomon rs(112);
  ccsds::Descrambler descrambler;

  std::vector<uint8_t> raw_cadus;
  size_t stream_offset = 0;
  uint8_t mc_cnt = 0;

  while (stream_offset < packet_stream.size()) {
    size_t frame_start_offset = stream_offset;
    size_t frame_end_offset = frame_start_offset + DATA_FIELD_LEN;

    // Calculate First Header Pointer (FHP) for this frame
    uint16_t fhp = 0x7FF;  // Default: no packet starts in this frame
    for (size_t p_offset : packet_start_offsets) {
      if (p_offset >= frame_start_offset && p_offset < frame_end_offset) {
        fhp = static_cast<uint16_t>(p_offset - frame_start_offset);
        break;
      }
    }

    size_t chunk_len = std::min(DATA_FIELD_LEN, packet_stream.size() - stream_offset);
    std::vector<uint8_t> data_field(DATA_FIELD_LEN, 0x00);
    std::copy(packet_stream.begin() + static_cast<std::ptrdiff_t>(stream_offset),
              packet_stream.begin() + static_cast<std::ptrdiff_t>(stream_offset + chunk_len),
              data_field.begin());

    ccsds::TransferFrameHeader hdr;
    hdr.version = 0;
    hdr.spacecraft_id = 0x123;
    hdr.virtual_channel_id = 1;
    hdr.master_frame_count = mc_cnt++;
    hdr.virtual_frame_count = hdr.master_frame_count;
    hdr.first_header_pointer = fhp;

    std::vector<uint8_t> tf_info =
        ccsds::TransferFrameParser::pack(hdr, data_field, std::nullopt, true);
    assert(tf_info.size() == TF_INFO_LEN);

    descrambler.process(tf_info);

    std::vector<uint8_t> tf_codeword(1020);
    for (size_t d = 0; d < RS_DEPTH; ++d) {
      std::vector<uint8_t> sub_info(223);
      for (size_t j = 0; j < 223; ++j) {
        sub_info[j] = tf_info[j * RS_DEPTH + d];
      }
      std::vector<uint8_t> sub_cw(255);
      rs.encode(sub_info, sub_cw);
      for (size_t j = 0; j < 255; ++j) {
        tf_codeword[j * RS_DEPTH + d] = sub_cw[j];
      }
    }

    raw_cadus.push_back(0x1A);
    raw_cadus.push_back(0xCF);
    raw_cadus.push_back(0xFC);
    raw_cadus.push_back(0x1D);
    raw_cadus.insert(raw_cadus.end(), tf_codeword.begin(), tf_codeword.end());

    stream_offset += chunk_len;
  }

  // Prepend 10 junk bytes to test SEARCH synchronization
  std::vector<uint8_t> noisy_stream = {0xFF, 0x00, 0xAA, 0x55, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
  noisy_stream.insert(noisy_stream.end(), raw_cadus.begin(), raw_cadus.end());

  // Inject 1 bit flip into the second frame's ASM (tolerated by ASM tolerance
  // 2)
  noisy_stream[10 + 1024] ^= 0x01;

  // Inject 8 byte errors into the first frame's data (RS will correct up to 16
  // errors/block)
  for (size_t b = 0; b < 8; ++b) {
    noisy_stream[10 + 4 + 100 + b * 4] ^= 0xCC;
  }

  auto decommutated_packets = decommutator.ingest(noisy_stream);

  std::cout << "Original packets sent: " << original_packets.size() << "\n";
  std::cout << "Packets decommutated:  " << decommutated_packets.size() << "\n";

  assert(decommutated_packets.size() == original_packets.size());
  for (size_t i = 0; i < original_packets.size(); ++i) {
    assert(decommutated_packets[i].apid == original_packets[i].apid);
    assert(decommutated_packets[i].sequence_count == original_packets[i].sequence_count);
    assert(decommutated_packets[i].payload == original_packets[i].payload);
  }

  auto stats = decommutator.stats();
  assert(stats.cadus_synchronized > 0);
  assert(stats.rs_corrected_symbols > 0);
  assert(stats.crc_passed_frames > 0);
  assert(stats.crc_failed_frames == 0);

  std::cout << "[PASS] End-to-end telemetry decommutation verified with 100% "
               "bit parity!\n";
  return 0;
}
