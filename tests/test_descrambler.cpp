#include "ccsds/descrambler.hpp"
#include <cassert>
#include <iostream>

void test_descrambler_inversion() {
  ccsds::Descrambler descrambler;

  std::vector<uint8_t> original = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                                   0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB};
  std::vector<uint8_t> processed = original;

  // First application: scramble
  descrambler.process(processed);
  assert(processed != original);

  // Second application: descramble (XOR is an involution)
  descrambler.process(processed);
  assert(processed == original);
}

void test_known_pn_sequence() {
  ccsds::Descrambler descrambler;
  const auto &seq = descrambler.sequence();
  assert(seq.size() == 255);

  // Initial state is 0xFF.
  // Let's verify that the sequence is non-trivial and contains roughly balanced
  // 1s and 0s
  size_t ones_count = 0;
  for (uint8_t b : seq) {
    for (int bit = 0; bit < 8; ++bit) {
      if ((b >> bit) & 1)
        ones_count++;
    }
  }
  // For a maximal-length sequence, roughly half the bits should be 1
  double ratio = static_cast<double>(ones_count) / (255 * 8);
  assert(ratio > 0.45 && ratio < 0.55);
}

int main() {
  std::cout << "[TEST] Running CCSDS Descrambler tests...\n";
  test_descrambler_inversion();
  test_known_pn_sequence();
  std::cout << "[PASS] Descrambler tests passed successfully.\n";
  return 0;
}
