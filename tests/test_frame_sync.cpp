#include "ccsds/frame_sync.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void test_hamming_distance() {
  assert(ccsds::FrameSynchronizer::hamming_distance(0x1ACFFC1D, 0x1ACFFC1D) ==
         0);
  assert(ccsds::FrameSynchronizer::hamming_distance(0x1ACFFC1D, 0x1ACFFC1C) ==
         1); // 1 bit flip (LSB)
  assert(ccsds::FrameSynchronizer::hamming_distance(0x00000000, 0xFFFFFFFF) ==
         32);
}

void test_sync_state_machine() {
  ccsds::DecommutatorConfig config;
  config.frame_size = 100; // 4-byte ASM + 96-byte payload
  config.asm_tolerance = 1;
  config.check_frames_required = 2;
  config.flywheel_max_frames = 2;

  ccsds::FrameSynchronizer sync(config);
  assert(sync.current_state() == ccsds::SyncState::SEARCH);

  // Construct a CADU
  auto make_cadu = [](uint32_t asm_val, uint8_t fill_val) {
    std::vector<uint8_t> c;
    c.push_back(static_cast<uint8_t>((asm_val >> 24) & 0xFF));
    c.push_back(static_cast<uint8_t>((asm_val >> 16) & 0xFF));
    c.push_back(static_cast<uint8_t>((asm_val >> 8) & 0xFF));
    c.push_back(static_cast<uint8_t>(asm_val & 0xFF));
    c.insert(c.end(), 96, fill_val);
    return c;
  };

  // Stream with junk bytes, then valid periodic CADUs
  std::vector<uint8_t> stream = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11};
  auto cadu1 = make_cadu(ccsds::FORWARD_ASM, 0x11);
  auto cadu2 = make_cadu(ccsds::FORWARD_ASM, 0x22);
  auto cadu3 = make_cadu(ccsds::FORWARD_ASM, 0x33);

  stream.insert(stream.end(), cadu1.begin(), cadu1.end());
  stream.insert(stream.end(), cadu2.begin(), cadu2.end());
  stream.insert(stream.end(), cadu3.begin(), cadu3.end());

  auto result = sync.process_stream(stream);

  // After feeding cadu1, cadu2, cadu3, synchronizer should enter LOCK and emit
  // CADUs
  assert(sync.current_state() == ccsds::SyncState::LOCK);
  assert(!result.empty());
}

void test_inverted_asm() {
  ccsds::DecommutatorConfig config;
  config.frame_size = 50;
  ccsds::FrameSynchronizer sync(config);

  // Construct an inverted CADU (180 deg phase shift)
  std::vector<uint8_t> stream;
  // Add two inverted CADUs to establish lock
  for (int i = 0; i < 3; ++i) {
    stream.push_back(0xE5);
    stream.push_back(0x30);
    stream.push_back(0x03);
    stream.push_back(0xE2);
    // Inverted payload: ~0xAA = 0x55
    stream.insert(stream.end(), 46, 0x55);
  }

  auto result = sync.process_stream(stream);
  assert(sync.current_state() == ccsds::SyncState::LOCK);
  assert(!result.empty());
  // Inverted CADU should be automatically reinverted to original phase (0xAA)
  for (uint8_t b : result[0].data) {
    assert(b == 0xAA);
  }
}

void test_flywheel_exhaustion() {
  ccsds::DecommutatorConfig config;
  config.frame_size = 50;
  config.check_frames_required = 1;
  config.flywheel_max_frames = 2;

  ccsds::FrameSynchronizer sync(config);

  // 1. Establish lock with 3 valid frames
  std::vector<uint8_t> stream;
  for (int i = 0; i < 3; ++i) {
    stream.push_back(0x1A);
    stream.push_back(0xCF);
    stream.push_back(0xFC);
    stream.push_back(0x1D);
    stream.insert(stream.end(), 46, 0x01);
  }

  auto res1 = sync.process_stream(stream);
  assert(sync.current_state() == ccsds::SyncState::LOCK);

  // 2. Feed corrupted ASMs to trigger FLYWHEEL
  std::vector<uint8_t> corrupted_stream;
  for (int i = 0; i < 4; ++i) {
    corrupted_stream.push_back(0x00);
    corrupted_stream.push_back(0x00);
    corrupted_stream.push_back(0x00);
    corrupted_stream.push_back(0x00);
    corrupted_stream.insert(corrupted_stream.end(), 46, 0x02);
  }

  auto res2 = sync.process_stream(corrupted_stream);
  // After 2 flywheel frames, the 3rd corruption drops to SEARCH
  assert(sync.current_state() == ccsds::SyncState::SEARCH);
}

int main() {
  std::cout << "[TEST] Running Frame Synchronizer tests...\n";
  test_hamming_distance();
  test_sync_state_machine();
  test_inverted_asm();
  test_flywheel_exhaustion();
  std::cout << "[PASS] Frame Synchronizer tests passed successfully.\n";
  return 0;
}
