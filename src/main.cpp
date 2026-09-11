#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "ccsds/decommutator.hpp"

void print_usage(const char* prog_name) {
  std::cout << "CCSDS Telemetry Frame Decommutator\n";
  std::cout << "Usage: " << prog_name << " [OPTIONS]\n\n";
  std::cout << "Options:\n";
  std::cout << "  -i, --input <file>       Path to raw telemetry binary input "
               "file (or - for stdin)\n";
  std::cout << "  -o, --output-apid        Print extracted Space Packet "
               "summary by APID\n";
  std::cout << "  -f, --frame-size <bytes> Total CADU frame size in bytes "
               "(default: 1024)\n";
  std::cout << "  -t, --asm-tol <bits>     Bit error tolerance for 32-bit ASM "
               "match (default: 1)\n";
  std::cout << "  --no-rs                  Disable Reed-Solomon (255, 223) "
               "decoding\n";
  std::cout << "  --no-descramble          Disable CCSDS pseudo-random "
               "descrambling\n";
  std::cout << "  --no-crc                 Disable CRC-16-CCITT FECF verification\n";
  std::cout << "  -h, --help               Display this help message\n";
}

int main(int argc, char* argv[]) {
  std::string input_path;
  bool print_apids = true;
  ccsds::DecommutatorConfig config;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      return 0;
    } else if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
      input_path = argv[++i];
    } else if (arg == "-o" || arg == "--output-apid") {
      print_apids = true;
    } else if ((arg == "-f" || arg == "--frame-size") && i + 1 < argc) {
      config.frame_size = std::stoul(argv[++i]);
    } else if ((arg == "-t" || arg == "--asm-tol") && i + 1 < argc) {
      config.asm_tolerance = std::stoul(argv[++i]);
    } else if (arg == "--no-rs") {
      config.enable_reed_solomon = false;
    } else if (arg == "--no-descramble") {
      config.enable_descrambler = false;
    } else if (arg == "--no-crc") {
      config.enable_crc_check = false;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      print_usage(argv[0]);
      return 1;
    }
  }

  if (input_path.empty()) {
    std::cout << "No input file specified. Run with -h for usage options.\n";
    return 0;
  }

  std::ifstream file(input_path, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open input file: " << input_path << "\n";
    return 1;
  }

  ccsds::Decommutator decommutator(config);

  std::cout << "=== CCSDS Telemetry Frame Decommutator ===\n";
  std::cout << "Ingesting stream from: " << input_path << "\n";
  std::cout << "CADU Size: " << config.frame_size
            << " bytes | ASM Tolerance: " << config.asm_tolerance << " bits\n\n";

  constexpr size_t CHUNK_SIZE = 64 * 1024;
  std::vector<uint8_t> buffer(CHUNK_SIZE);

  size_t packet_count = 0;
  std::map<uint16_t, size_t> apid_counts;

  decommutator.set_packet_callback([&](const ccsds::SpacePacket& pkt,
                                       const ccsds::TransferFrameHeader& tf) {
    ++packet_count;
    apid_counts[pkt.apid]++;
    if (print_apids && packet_count <= 20) {
      std::cout << "[PACKET #" << std::setw(4) << packet_count << "] ";
      std::cout << "APID: " << std::setw(4) << pkt.apid << " | ";
      std::cout << "VCID: " << static_cast<int>(tf.virtual_channel_id) << " | ";
      std::cout << "MC_CNT: " << std::setw(3) << static_cast<int>(tf.master_frame_count) << " | ";
      std::cout << "SEQ: " << std::setw(5) << pkt.sequence_count << " | ";
      std::cout << "Payload: " << pkt.payload.size() << " bytes\n";
    }
  });

  while (file.read(reinterpret_cast<char*>(buffer.data()), CHUNK_SIZE) || file.gcount() > 0) {
    size_t bytes_read = static_cast<size_t>(file.gcount());
    decommutator.ingest(std::span<const uint8_t>(buffer.data(), bytes_read));
  }

  auto stats = decommutator.stats();

  std::cout << "\n=== Decommutation Session Summary ===\n";
  std::cout << "Bytes Ingested:        " << stats.bytes_ingested << " bytes\n";
  std::cout << "CADUs Synchronized:    " << stats.cadus_synchronized << "\n";
  std::cout << "ASM Bit Errors:        " << stats.asm_bit_errors << "\n";
  std::cout << "RS Corrected Symbols:  " << stats.rs_corrected_symbols << "\n";
  std::cout << "RS Uncorrectable:      " << stats.rs_uncorrectable_frames << "\n";
  std::cout << "CRC Passed Frames:     " << stats.crc_passed_frames << "\n";
  std::cout << "CRC Failed Frames:     " << stats.crc_failed_frames << "\n";
  std::cout << "Packets Decommutated:  " << stats.packets_extracted << "\n";

  if (!apid_counts.empty()) {
    std::cout << "\n--- Decommutated APID Breakdown ---\n";
    for (const auto& [apid, count] : apid_counts) {
      std::cout << "  APID 0x" << std::hex << std::setw(3) << std::setfill('0') << apid << " ("
                << std::dec << apid << "): " << count << " packets\n";
    }
  }

  return 0;
}
