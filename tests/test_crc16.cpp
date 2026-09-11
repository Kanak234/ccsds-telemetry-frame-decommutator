#include <cassert>
#include <iostream>
#include <vector>

#include "ccsds/crc16.hpp"

void test_crc16_computation() {
  ccsds::Crc16 crc;

  // Test with standard test vector
  std::vector<uint8_t> test_data = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  uint16_t val = crc.compute(test_data);
  // For 0x1021 with init 0xFFFF, no reflection, final XOR 0x0000:
  // ASCII "123456789" produces 0x29B1
  assert(val == 0x29B1);
}

void test_crc16_verify() {
  ccsds::Crc16 crc;

  std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
  uint16_t calculated = crc.compute(payload);

  std::vector<uint8_t> frame_with_crc = payload;
  frame_with_crc.push_back(static_cast<uint8_t>((calculated >> 8) & 0xFF));
  frame_with_crc.push_back(static_cast<uint8_t>(calculated & 0xFF));

  // Valid frame check
  assert(crc.verify(frame_with_crc));

  // Corrupted payload check
  frame_with_crc[2] ^= 0x01;
  assert(!crc.verify(frame_with_crc));

  // Corrupted CRC check
  frame_with_crc[2] ^= 0x01;      // restore payload
  frame_with_crc.back() ^= 0xFF;  // corrupt CRC
  assert(!crc.verify(frame_with_crc));
}

int main() {
  std::cout << "[TEST] Running CRC-16-CCITT tests...\n";
  test_crc16_computation();
  test_crc16_verify();
  std::cout << "[PASS] CRC-16 tests passed successfully.\n";
  return 0;
}
