#ifndef CCSDS_FRAME_SYNC_HPP
#define CCSDS_FRAME_SYNC_HPP

#include "ccsds/types.hpp"
#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace ccsds {

/**
 * Implements the standard CCSDS 4-State Frame Synchronizer State Machine
 * (SEARCH -> CHECK -> LOCK -> FLYWHEEL).
 */
class FrameSynchronizer {
public:
  explicit FrameSynchronizer(const DecommutatorConfig &config = {});

  // Ingests a chunk of raw byte stream telemetry and extracts any fully
  // synchronized CADUs.
  std::vector<Cadu> process_stream(std::span<const uint8_t> stream);

  // Flushes any buffered bytes and resets the state machine back to SEARCH.
  void reset();

  // Accessors for state inspection and testing
  SyncState current_state() const noexcept { return state_; }
  size_t buffer_size() const noexcept { return buffer_.size(); }
  const PipelineStats &stats() const noexcept { return stats_; }

  // Calculates the bit Hamming distance between two 32-bit values
  static constexpr size_t hamming_distance(uint32_t a, uint32_t b) noexcept {
    return static_cast<size_t>(std::popcount(a ^ b));
  }

private:
  DecommutatorConfig config_;
  SyncState state_{SyncState::SEARCH};
  std::vector<uint8_t> buffer_;
  size_t check_count_{0};
  size_t flywheel_count_{0};
  PipelineStats stats_{};

  // Checks whether 4 bytes in the buffer form an ASM within bit tolerance
  struct AsmMatch {
    bool matched{false};
    bool inverted{false};
    size_t bit_errors{0};
  };
  AsmMatch check_asm_at(size_t offset) const;

  // Handles processing for each state
  void process_search(std::vector<Cadu> &output);
  void process_check(std::vector<Cadu> &output);
  void process_lock(std::vector<Cadu> &output);
  void process_flywheel(std::vector<Cadu> &output);
};

} // namespace ccsds

#endif // CCSDS_FRAME_SYNC_HPP
