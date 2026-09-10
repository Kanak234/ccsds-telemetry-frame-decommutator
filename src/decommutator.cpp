#include "ccsds/decommutator.hpp"

namespace ccsds {

Decommutator::Decommutator(const DecommutatorConfig &config)
    : config_(config), synchronizer_(config),
      rs_decoder_(112), // Standard CCSDS first root
      descrambler_(), tf_parser_(config.enable_crc_check), packet_extractor_() {
}

void Decommutator::reset() {
  synchronizer_.reset();
  packet_extractor_.reset();
  stats_ = PipelineStats{};
}

PipelineStats Decommutator::stats() const {
  PipelineStats combined = stats_;
  const auto &sync_stats = synchronizer_.stats();
  combined.bytes_ingested = sync_stats.bytes_ingested;
  combined.cadus_synchronized = sync_stats.cadus_synchronized;
  combined.asm_bit_errors = sync_stats.asm_bit_errors;
  return combined;
}

std::vector<SpacePacket> Decommutator::ingest(std::span<const uint8_t> stream) {
  std::vector<SpacePacket> extracted_packets;

  // Stage 1: Frame Synchronization (Locate CADUs using 32-bit ASM)
  std::vector<Cadu> cadus = synchronizer_.process_stream(stream);

  for (auto &cadu : cadus) {
    // Stage 2: Reed-Solomon Forward Error Correction (if enabled)
    if (config_.enable_reed_solomon) {
      size_t depth = cadu.data.size() / RS_CODEWORD_SIZE;
      if (depth > 0 && cadu.data.size() == depth * RS_CODEWORD_SIZE) {
        size_t corrected_errors = 0;
        bool rs_ok =
            rs_decoder_.decode_interleaved(cadu.data, depth, corrected_errors);
        if (!rs_ok) {
          stats_.rs_uncorrectable_frames++;
          continue; // Skip corrupted uncorrectable frame
        }
        stats_.rs_corrected_symbols += corrected_errors;
        // Strip the parity symbols: Keep only the information payload (depth *
        // 223 bytes)
        cadu.data.resize(depth * RS_DATA_SIZE);
      } else if (cadu.data.size() == RS_CODEWORD_SIZE) {
        RsDecodeResult rs_res = rs_decoder_.decode(cadu.data);
        if (!rs_res.success) {
          stats_.rs_uncorrectable_frames++;
          continue;
        }
        stats_.rs_corrected_symbols += rs_res.errors_corrected;
        cadu.data.resize(RS_DATA_SIZE);
      }
    }

    std::span<uint8_t> tf_data(cadu.data);

    // Stage 3: Pseudo-Random Descrambling (if enabled)
    if (config_.enable_descrambler) {
      descrambler_.process(tf_data);
    }

    // Stage 4: Transfer Frame Header & CRC Validation
    auto opt_unpacked = tf_parser_.parse(tf_data);
    if (!opt_unpacked.has_value() || !opt_unpacked->valid) {
      stats_.crc_failed_frames++;
      continue;
    }
    stats_.crc_passed_frames++;

    const auto &unpacked = opt_unpacked.value();

    // Stage 5: Space Packet Extraction & Decommutation
    std::vector<SpacePacket> packets = packet_extractor_.ingest_frame_data(
        unpacked.header.virtual_channel_id,
        unpacked.header.first_header_pointer, unpacked.data_field);

    for (const auto &pkt : packets) {
      stats_.packets_extracted++;
      if (callback_) {
        callback_(pkt, unpacked.header);
      }
      extracted_packets.push_back(pkt);
    }
  }

  return extracted_packets;
}

} // namespace ccsds
