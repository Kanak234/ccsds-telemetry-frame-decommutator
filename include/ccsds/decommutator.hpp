#ifndef CCSDS_DECOMMUTATOR_HPP
#define CCSDS_DECOMMUTATOR_HPP

#include "ccsds/descrambler.hpp"
#include "ccsds/frame_sync.hpp"
#include "ccsds/packet_extractor.hpp"
#include "ccsds/reed_solomon.hpp"
#include "ccsds/transfer_frame.hpp"
#include "ccsds/types.hpp"
#include <functional>
#include <span>
#include <vector>

namespace ccsds {

// Callback type invoked when a completed Space Packet is decommutated
using PacketCallback =
    std::function<void(const SpacePacket &, const TransferFrameHeader &)>;

/**
 * High-level orchestration engine for end-to-end CCSDS telemetry decommutation.
 */
class Decommutator {
public:
  explicit Decommutator(const DecommutatorConfig &config = {});

  // Ingests a raw byte chunk from a telemetry stream or file, processing it
  // through synchronization, Reed-Solomon error correction, descrambling, frame
  // unpacking, and packet extraction.
  std::vector<SpacePacket> ingest(std::span<const uint8_t> stream);

  // Registers a streaming callback invoked immediately upon each completed
  // Space Packet
  void set_packet_callback(PacketCallback callback) {
    callback_ = std::move(callback);
  }

  // Resets internal state across all pipeline stages
  void reset();

  // Returns cumulative operational metrics across the session
  PipelineStats stats() const;

  const DecommutatorConfig &config() const noexcept { return config_; }

private:
  DecommutatorConfig config_;
  FrameSynchronizer synchronizer_;
  ReedSolomon rs_decoder_;
  Descrambler descrambler_;
  TransferFrameParser tf_parser_;
  PacketExtractor packet_extractor_;
  PipelineStats stats_{};
  PacketCallback callback_;
};

} // namespace ccsds

#endif // CCSDS_DECOMMUTATOR_HPP
