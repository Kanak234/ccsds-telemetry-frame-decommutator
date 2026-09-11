#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

#include "ccsds/reed_solomon.hpp"

void test_clean_codeword() {
  ccsds::ReedSolomon rs(112);

  std::vector<uint8_t> message(223);
  for (size_t i = 0; i < 223; ++i) {
    message[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);
  }

  std::vector<uint8_t> codeword(255);
  rs.encode(message, codeword);

  for (size_t i = 0; i < 223; ++i) {
    assert(codeword[i] == message[i]);
  }

  ccsds::RsDecodeResult res = rs.decode(codeword);
  assert(res.success);
  assert(!res.corrected);
  assert(res.errors_corrected == 0);
}

void test_error_correction() {
  ccsds::ReedSolomon rs(112);

  std::vector<uint8_t> message(223);
  for (size_t i = 0; i < 223; ++i) {
    message[i] = static_cast<uint8_t>((i * 31 + 5) & 0xFF);
  }

  std::vector<uint8_t> original_codeword(255);
  rs.encode(message, original_codeword);

  std::vector<size_t> test_error_counts = {1, 4, 8, 12, 16};
  std::mt19937 rng(42);

  for (size_t err_count : test_error_counts) {
    std::vector<uint8_t> corrupted = original_codeword;

    std::vector<size_t> positions(255);
    for (size_t i = 0; i < 255; ++i) positions[i] = i;
    std::shuffle(positions.begin(), positions.end(), rng);

    for (size_t e = 0; e < err_count; ++e) {
      size_t pos = positions[e];
      corrupted[pos] ^= static_cast<uint8_t>((rng() % 254) + 1);
    }

    ccsds::RsDecodeResult res = rs.decode(corrupted);
    assert(res.success);
    assert(res.corrected);
    assert(res.errors_corrected == err_count);
    assert(corrupted == original_codeword);
  }
}

void test_uncorrectable_errors() {
  ccsds::ReedSolomon rs(112);

  std::vector<uint8_t> message(223, 0x42);
  std::vector<uint8_t> codeword(255);
  rs.encode(message, codeword);

  // Inject 18 symbol errors (> 16 error capability)
  std::mt19937 rng(1337);
  for (size_t i = 0; i < 18; ++i) {
    codeword[i * 10] ^= 0xAA;
  }

  ccsds::RsDecodeResult res = rs.decode(codeword);
  assert(!res.success);
}

void test_interleaved_rs() {
  ccsds::ReedSolomon rs(112);
  size_t depth = 4;
  size_t total_size = depth * 255;
  std::vector<uint8_t> interleaved(total_size);

  for (size_t d = 0; d < depth; ++d) {
    std::vector<uint8_t> msg(223, static_cast<uint8_t>(d + 1));
    std::vector<uint8_t> cw(255);
    rs.encode(msg, cw);
    for (size_t j = 0; j < 255; ++j) {
      interleaved[j * depth + d] = cw[j];
    }
  }

  std::vector<uint8_t> clean_interleaved = interleaved;

  for (size_t i = 50; i < 66; ++i) {
    interleaved[i] ^= 0x55;
  }

  size_t corrected = 0;
  bool ok = rs.decode_interleaved(interleaved, depth, corrected);
  assert(ok);
  assert(corrected == 16);
  assert(interleaved == clean_interleaved);
}

int main() {
  std::cout << "[TEST] Running Reed-Solomon (255, 223) tests...\n";
  test_clean_codeword();
  test_error_correction();
  test_uncorrectable_errors();
  test_interleaved_rs();
  std::cout << "[PASS] Reed-Solomon tests passed successfully.\n";
  return 0;
}
