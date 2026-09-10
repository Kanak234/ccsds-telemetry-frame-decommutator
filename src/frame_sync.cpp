#include "ccsds/frame_sync.hpp"
#include <cstring>

namespace ccsds {

FrameSynchronizer::FrameSynchronizer(const DecommutatorConfig &config)
    : config_(config) {}

void FrameSynchronizer::reset() {
  state_ = SyncState::SEARCH;
  buffer_.clear();
  check_count_ = 0;
  flywheel_count_ = 0;
}

FrameSynchronizer::AsmMatch
FrameSynchronizer::check_asm_at(size_t offset) const {
  if (offset + ASM_SIZE > buffer_.size()) {
    return {false, false, 0};
  }

  // Read 32-bit big-endian value from buffer
  uint32_t val = (static_cast<uint32_t>(buffer_[offset]) << 24) |
                 (static_cast<uint32_t>(buffer_[offset + 1]) << 16) |
                 (static_cast<uint32_t>(buffer_[offset + 2]) << 8) |
                 static_cast<uint32_t>(buffer_[offset + 3]);

  size_t fwd_err = hamming_distance(val, FORWARD_ASM);
  if (fwd_err <= config_.asm_tolerance) {
    return {true, false, fwd_err};
  }

  size_t inv_err = hamming_distance(val, INVERTED_ASM);
  if (inv_err <= config_.asm_tolerance) {
    return {true, true, inv_err};
  }

  return {false, false, 0};
}

std::vector<Cadu>
FrameSynchronizer::process_stream(std::span<const uint8_t> stream) {
  stats_.bytes_ingested += stream.size();
  buffer_.insert(buffer_.end(), stream.begin(), stream.end());

  std::vector<Cadu> output_cadus;

  bool state_changed = true;
  while (state_changed && buffer_.size() >= config_.frame_size) {
    SyncState old_state = state_;

    switch (state_) {
    case SyncState::SEARCH:
      process_search(output_cadus);
      break;
    case SyncState::CHECK:
      process_check(output_cadus);
      break;
    case SyncState::LOCK:
      process_lock(output_cadus);
      break;
    case SyncState::FLYWHEEL:
      process_flywheel(output_cadus);
      break;
    }

    state_changed = (state_ != old_state);
  }

  return output_cadus;
}

// SEARCH State: Scan the byte buffer until an ASM candidate is found.
// Why it was written: In cold start or lost sync, bitstream phase is unknown.
// Finding the first candidate anchors the tentative frame boundary.
void FrameSynchronizer::process_search(std::vector<Cadu> & /*output*/) {
  size_t scan_limit =
      buffer_.size() >= ASM_SIZE ? buffer_.size() - ASM_SIZE + 1 : 0;

  for (size_t i = 0; i < scan_limit; ++i) {
    AsmMatch match = check_asm_at(i);
    if (match.matched) {
      // Discard any leading un-synchronized junk bytes before the ASM
      if (i > 0) {
        buffer_.erase(buffer_.begin(),
                      buffer_.begin() + static_cast<std::ptrdiff_t>(i));
      }
      state_ = SyncState::CHECK;
      check_count_ = 1;
      return;
    }
  }

  // If no ASM found, keep only the last 3 bytes (in case an ASM was split
  // across chunks)
  if (buffer_.size() >= ASM_SIZE) {
    size_t bytes_to_discard = buffer_.size() - (ASM_SIZE - 1);
    buffer_.erase(buffer_.begin(),
                  buffer_.begin() +
                      static_cast<std::ptrdiff_t>(bytes_to_discard));
  }
}

// CHECK State: Verify that the expected periodic frame boundary contains an
// ASM. Why it was written: Random data can occasionally mimic the 32-bit ASM.
// Verifying periodicity prevents false lock acquisitions on random payload
// data.
void FrameSynchronizer::process_check(std::vector<Cadu> &output) {
  if (buffer_.size() < config_.frame_size + ASM_SIZE) {
    return; // Need more data to verify next boundary
  }

  AsmMatch next_match = check_asm_at(config_.frame_size);
  if (next_match.matched) {
    ++check_count_;
    if (check_count_ >= config_.check_frames_required) {
      state_ = SyncState::LOCK;
      flywheel_count_ = 0;
      // Now in LOCK: extract the first CADU
      process_lock(output);
    } else {
      // Still in CHECK, drop the current frame and check next
      buffer_.erase(buffer_.begin(),
                    buffer_.begin() +
                        static_cast<std::ptrdiff_t>(config_.frame_size));
    }
  } else {
    // Periodic check failed: false alarm! Discard first byte and return to
    // SEARCH
    buffer_.erase(buffer_.begin());
    state_ = SyncState::SEARCH;
    check_count_ = 0;
  }
}

// LOCK State: Stream is synchronized. Extract CADUs continuously.
// Why it was written: In steady state, maximum processing throughput is
// achieved by slicing fixed-length CADU chunks directly without rescanning.
void FrameSynchronizer::process_lock(std::vector<Cadu> &output) {
  while (buffer_.size() >= config_.frame_size) {
    AsmMatch current_match = check_asm_at(0);

    if (current_match.matched) {
      Cadu cadu;
      cadu.asm_pattern = FORWARD_ASM;
      cadu.inverted = current_match.inverted;
      cadu.bit_errors = current_match.bit_errors;
      // Copy payload (excluding 4-byte ASM)
      cadu.data.assign(buffer_.begin() + static_cast<std::ptrdiff_t>(ASM_SIZE),
                       buffer_.begin() +
                           static_cast<std::ptrdiff_t>(config_.frame_size));

      // If inverted, invert all bits in payload to restore true phase
      if (cadu.inverted) {
        for (uint8_t &b : cadu.data) {
          b ^= 0xFF;
        }
      }

      output.push_back(std::move(cadu));
      stats_.cadus_synchronized++;
      stats_.asm_bit_errors += current_match.bit_errors;

      // Remove processed frame from buffer
      buffer_.erase(buffer_.begin(),
                    buffer_.begin() +
                        static_cast<std::ptrdiff_t>(config_.frame_size));
      flywheel_count_ = 0; // Reset flywheel on successful sync
    } else {
      // Expected ASM missing! Transition to FLYWHEEL to ride through temporary
      // signal fade
      state_ = SyncState::FLYWHEEL;
      flywheel_count_ = 1;
      process_flywheel(output);
      return;
    }
  }
}

// FLYWHEEL State: Synthesize frame boundaries during temporary signal dropouts.
// Why it was written: Atmospheric scintillation or antenna tracking slews can
// corrupt ASMs. Flywheeling keeps downstream telemetry processing alive during
// momentary fades.
void FrameSynchronizer::process_flywheel(std::vector<Cadu> &output) {
  while (buffer_.size() >= config_.frame_size &&
         state_ == SyncState::FLYWHEEL) {
    AsmMatch current_match = check_asm_at(0);

    if (current_match.matched) {
      // Signal re-acquired! Transition back to LOCK
      state_ = SyncState::LOCK;
      flywheel_count_ = 0;
      process_lock(output);
      return;
    }

    if (flywheel_count_ <= config_.flywheel_max_frames) {
      // Synthesize frame
      Cadu cadu;
      cadu.asm_pattern = FORWARD_ASM;
      cadu.inverted = false;
      cadu.bit_errors = 32; // Fully synthesized
      cadu.data.assign(buffer_.begin() + static_cast<std::ptrdiff_t>(ASM_SIZE),
                       buffer_.begin() +
                           static_cast<std::ptrdiff_t>(config_.frame_size));
      output.push_back(std::move(cadu));

      buffer_.erase(buffer_.begin(),
                    buffer_.begin() +
                        static_cast<std::ptrdiff_t>(config_.frame_size));
      ++flywheel_count_;
    } else {
      // Flywheel exceeded: Loss of Synchronization (LOS). Return to SEARCH
      state_ = SyncState::SEARCH;
      flywheel_count_ = 0;
      check_count_ = 0;
      return;
    }
  }
}

} // namespace ccsds
