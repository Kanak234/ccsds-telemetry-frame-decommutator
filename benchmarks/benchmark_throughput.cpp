#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#include "ccsds/decommutator.hpp"

int main() {
  std::cout << "=== Running CCSDS Decommutator Performance Benchmark ===\n";

  constexpr size_t NUM_FRAMES = 5000;
  constexpr size_t FRAME_SIZE = 1024;
  constexpr size_t RS_DEPTH = 4;
  constexpr size_t TF_INFO_LEN = RS_DEPTH * 223;  // 892 bytes
  constexpr size_t DATA_FIELD_LEN = TF_INFO_LEN - ccsds::TF_PRIMARY_HEADER_SIZE - 2;

  ccsds::ReedSolomon rs(112);
  ccsds::Descrambler descrambler;
  ccsds::Crc16 crc16;

  // Pre-generate a stream of valid CADUs
  std::vector<uint8_t> stream;
  stream.reserve(NUM_FRAMES * FRAME_SIZE);

  ccsds::SpacePacket sample_pkt;
  sample_pkt.apid = 0x150;
  sample_pkt.sequence_flags = 3;
  sample_pkt.payload = std::vector<uint8_t>(200, 0x5A);
  std::vector<uint8_t> raw_pkt = ccsds::PacketExtractor::serialize_packet(sample_pkt);

  std::vector<uint8_t> data_field(DATA_FIELD_LEN, 0x00);
  std::copy(raw_pkt.begin(), raw_pkt.end(), data_field.begin());

  for (size_t f = 0; f < NUM_FRAMES; ++f) {
    ccsds::TransferFrameHeader hdr;
    hdr.version = 0;
    hdr.spacecraft_id = 42;
    hdr.virtual_channel_id = 2;
    hdr.master_frame_count = static_cast<uint8_t>(f & 0xFF);
    hdr.virtual_frame_count = static_cast<uint8_t>(f & 0xFF);
    hdr.first_header_pointer = 0;

    std::vector<uint8_t> tf_info =
        ccsds::TransferFrameParser::pack(hdr, data_field, std::nullopt, true);
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

    stream.push_back(0x1A);
    stream.push_back(0xCF);
    stream.push_back(0xFC);
    stream.push_back(0x1D);
    stream.insert(stream.end(), tf_codeword.begin(), tf_codeword.end());
  }

  ccsds::DecommutatorConfig config;
  config.frame_size = FRAME_SIZE;
  config.asm_tolerance = 1;
  config.enable_descrambler = true;
  config.enable_reed_solomon = true;
  config.enable_crc_check = true;

  ccsds::Decommutator decommutator(config);

  // Warm-up run
  decommutator.ingest(std::span<const uint8_t>(stream.data(), FRAME_SIZE * 10));
  decommutator.reset();

  std::cout << "Ingesting " << NUM_FRAMES << " CADU frames ("
            << (NUM_FRAMES * FRAME_SIZE) / (1024.0 * 1024.0) << " MB)...\n";

  auto start = std::chrono::high_resolution_clock::now();
  auto packets = decommutator.ingest(stream);
  auto end = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double> elapsed = end - start;
  double seconds = elapsed.count();
  double mb_total = static_cast<double>(NUM_FRAMES * FRAME_SIZE) / (1024.0 * 1024.0);
  double mb_per_sec = mb_total / seconds;
  double frames_per_sec = static_cast<double>(NUM_FRAMES) / seconds;

  std::cout << "\n--- Benchmark Results ---\n";
  std::cout << "Elapsed Time:      " << std::fixed << std::setprecision(4) << seconds << " s\n";
  std::cout << "Throughput:        " << std::fixed << std::setprecision(2) << mb_per_sec
            << " MB/s\n";
  std::cout << "Frame Processing:  " << std::fixed << std::setprecision(1) << frames_per_sec
            << " frames/sec\n";
  std::cout << "Packets Extracted: " << packets.size() << "\n";
  std::cout << "Mean Latency:      " << std::fixed << std::setprecision(2)
            << (seconds / NUM_FRAMES) * 1e6 << " us/frame\n";

  return 0;
}
