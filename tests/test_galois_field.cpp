#include "ccsds/galois_field.hpp"
#include <cassert>
#include <iostream>

void test_addition_subtraction() {
  // In GF(2^8), addition and subtraction are XOR
  assert(ccsds::GaloisField::add(0x57, 0x83) == (0x57 ^ 0x83));
  assert(ccsds::GaloisField::sub(0x57, 0x83) == (0x57 ^ 0x83));
  assert(ccsds::GaloisField::add(0xAA, 0x00) == 0xAA);
  assert(ccsds::GaloisField::add(0xFF, 0xFF) == 0x00);
}

void test_multiplication_inversion() {
  ccsds::GaloisField gf;

  // Multiplication with zero
  assert(gf.mul(0, 42) == 0);
  assert(gf.mul(42, 0) == 0);

  // Multiplication with identity 1
  for (int i = 1; i < 256; ++i) {
    uint8_t a = static_cast<uint8_t>(i);
    assert(gf.mul(a, 1) == a);
    assert(gf.mul(1, a) == a);

    // Every non-zero element must have an inverse a * a^-1 == 1
    uint8_t inv = gf.inv(a);
    assert(gf.mul(a, inv) == 1);
    assert(gf.div(a, a) == 1);
  }

  // Commutativity
  assert(gf.mul(0x53, 0xCA) == gf.mul(0xCA, 0x53));
}

void test_exp_log_consistency() {
  ccsds::GaloisField gf;

  // alpha^0 = 1, alpha^255 = 1 (cyclic group of order 255)
  assert(gf.exp(0) == 1);
  assert(gf.exp(255) == 1);
  assert(gf.exp(510) == 1);

  for (int i = 1; i < 256; ++i) {
    uint8_t val = static_cast<uint8_t>(i);
    uint8_t log_val = gf.log(val);
    assert(gf.exp(log_val) == val);
  }
}

void test_polynomial_ops() {
  ccsds::GaloisField gf;

  // P(x) = 3 + 2*x + x^2
  std::vector<uint8_t> p = {3, 2, 1};
  // P(0) = 3
  assert(gf.poly_eval(p, 0) == 3);

  // Multiply (x + 1) * (x + 1) in GF(2^8)[x] = x^2 + (1+1)*x + 1 = x^2 + 1
  std::vector<uint8_t> p1 = {1, 1};
  std::vector<uint8_t> p2 = {1, 1};
  std::vector<uint8_t> prod = gf.poly_mul(p1, p2);
  assert(prod.size() == 3);
  assert(prod[0] == 1);
  assert(prod[1] == 0); // 1 + 1 = 0
  assert(prod[2] == 1);
}

int main() {
  std::cout << "[TEST] Running Galois Field GF(2^8) tests...\n";
  test_addition_subtraction();
  test_multiplication_inversion();
  test_exp_log_consistency();
  test_polynomial_ops();
  std::cout << "[PASS] Galois Field tests passed successfully.\n";
  return 0;
}
