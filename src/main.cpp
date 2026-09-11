#include <atomic>
#include <climits>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "ccsds/decommutator.hpp"

namespace {

std::atomic<bool> g_stopRequested{false};

/**
 * Signal handler for SIGINT and SIGTERM to gracefully interrupt telemetry processing.
 */
void signalHandler(int signum) {
  (void)signum;
  g_stopRequested.store(true);
}

/**
 * Validates and canonicalizes input file paths.
 */
std::string sanitizePath(const std::string& input) {
  if (input.empty()) {
    throw std::invalid_argument("Input path cannot be empty");
  }
  if (input == "-") {
    return input;  // stdin special indicator
  }
  if (input.find('\0') != std::string::npos) {
    throw std::invalid_argument("Input path contains invalid null character");
  }
  char resolved[PATH_MAX];
  if (realpath(input.c_str(), resolved) != nullptr) {
    return std::string(resolved);
  }
  std::filesystem::path p(input);
  std::error_code ec;
  auto norm = std::filesystem::weakly_canonical(p, ec);
  if (!ec) {
    return norm.string();
  }
  return p.lexically_normal().string();
}

/**
 * Prints CLI usage instructions and command line options.
 */
void print_usage(const char* prog_name) {
  std::cout << "CCSDS Telemetry Frame Decommutator\n";
  std::cout << "Usage: " << prog_name << " [OPTIONS]\n\n";
  std::cout << "Options:\n";
  std::cout
      << "  -i, --input <file>       Path to raw telemetry binary input file (or - for stdin)\n";
  std::cout << "  -o, --output-apid        Print extracted Space Packet summary by APID\n";
  std::cout << "  -f, --frame-size <bytes> Total CADU frame size in bytes (default: 1024)\n";
  std::cout << "  -t, --asm-tol <bits>     Bit error tolerance for 32-bit ASM match (default: 1)\n";
  std::cout << "  --no-rs                  Disable Reed-Solomon (255, 223) decoding\n";
  std::cout << "  --no-descramble          Disable CCSDS pseudo-random descrambling\n";
  std::cout << "  --no-crc                 Disable CRC-16-CCITT FECF verification\n";
  std::cout << "  health                   Run self-diagnostic health check\n";
  std::cout << "  -v, --version            Show version\n";
  std::cout << "  -h, --help               Display this help message\n";
}

/**
 * Executes a self-diagnostic decommutation check on an in-memory CADU stream.
 */
int cmdHealth() {
  try {
    ccsds::DecommutatorConfig config;
    config.frame_size = 259;
    config.enable_reed_solomon = true;
    config.enable_descrambler = false;
    config.enable_crc_check = false;

    ccsds::Decommutator decomm(config);

    ccsds::SpacePacket pkt;
    pkt.apid = 0x101;
    pkt.sequence_count = 1;
    pkt.sequence_flags = 3;
    pkt.payload = {0x11, 0x22, 0x33, 0x44};
    pkt.packet_data_length = static_cast<uint16_t>(pkt.payload.size() - 1);
    std::vector<uint8_t> pkt_bytes = ccsds::PacketExtractor::serialize_packet(pkt);

    std::vector<uint8_t> data_field(223 - ccsds::TF_PRIMARY_HEADER_SIZE, 0x00);
    std::copy(pkt_bytes.begin(), pkt_bytes.end(), data_field.begin());

    ccsds::TransferFrameHeader tf_hdr;
    tf_hdr.virtual_channel_id = 0;
    tf_hdr.master_frame_count = 0;
    tf_hdr.first_header_pointer = 0;

    std::vector<uint8_t> tf_info =
        ccsds::TransferFrameParser::pack(tf_hdr, data_field, std::nullopt, false);
    ccsds::ReedSolomon rs(112);
    std::vector<uint8_t> codeword(255);
    rs.encode(tf_info, codeword);

    std::vector<uint8_t> cadu = {0x1A, 0xCF, 0xFC, 0x1D};
    cadu.insert(cadu.end(), codeword.begin(), codeword.end());

    std::vector<uint8_t> stream;
    for (int i = 0; i < 4; ++i) {
      stream.insert(stream.end(), cadu.begin(), cadu.end());
    }

    auto extracted = decomm.ingest(stream);
    if (extracted.empty() || extracted[0].apid != 0x101) {
      std::cerr << "Health check failed: expected packet not extracted\n";
      return 1;
    }

    std::cout
        << "{\"status\":\"healthy\",\"engine\":\"ccsds_decommutator\",\"version\":\"1.0.0\"}\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Health check error: " << e.what() << "\n";
    return 1;
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  if (argc > 1) {
    std::string first_arg = argv[1];
    if (first_arg == "health") {
      return cmdHealth();
    }
    if (first_arg == "-v" || first_arg == "--version" || first_arg == "version") {
      std::cout << "ccsds_decommutator 1.0.0\n";
      return 0;
    }
  }

  std::string input_path;
  bool print_apids = true;
  ccsds::DecommutatorConfig config;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      return 0;
    } else if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
      input_path = sanitizePath(argv[++i]);
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

  while (!g_stopRequested.load() &&
         (file.read(reinterpret_cast<char*>(buffer.data()), CHUNK_SIZE) || file.gcount() > 0)) {
    size_t bytes_read = static_cast<size_t>(file.gcount());
    decommutator.ingest(std::span<const uint8_t>(buffer.data(), bytes_read));
  }

  if (g_stopRequested.load()) {
    std::cout << "\nProcessing interrupted by signal.\n";
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
