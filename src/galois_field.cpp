#include "ccsds/galois_field.hpp"
#include <cassert>

namespace ccsds {

void GaloisField::init_tables() {
  constexpr uint16_t primitive_poly = 0x187;

  uint16_t current = 1;
  for (size_t i = 0; i < 255; ++i) {
    exp_table_[i] = static_cast<uint8_t>(current);
    log_table_[current] = static_cast<uint8_t>(i);

    current <<= 1;
    if (current & 0x100) {
      current ^= primitive_poly;
    }
  }

  for (size_t i = 255; i < 512; ++i) {
    exp_table_[i] = exp_table_[i - 255];
  }
}

GaloisField::GaloisField() { init_tables(); }

uint8_t GaloisField::mul(uint8_t a, uint8_t b) const noexcept {
  if (a == 0 || b == 0) {
    return 0;
  }
  size_t idx =
      static_cast<size_t>(log_table_[a]) + static_cast<size_t>(log_table_[b]);
  return exp_table_[idx];
}

uint8_t GaloisField::div(uint8_t a, uint8_t b) const noexcept {
  if (a == 0) {
    return 0;
  }
  assert(b != 0 && "Division by zero in GF(2^8)");
  int diff = static_cast<int>(log_table_[a]) - static_cast<int>(log_table_[b]);
  if (diff < 0) {
    diff += 255;
  }
  return exp_table_[static_cast<size_t>(diff)];
}

uint8_t GaloisField::inv(uint8_t a) const noexcept {
  assert(a != 0 && "Zero element has no multiplicative inverse");
  return exp_table_[255 - static_cast<size_t>(log_table_[a])];
}

uint8_t GaloisField::exp(int power) const noexcept {
  int mod_power = power % 255;
  if (mod_power < 0) {
    mod_power += 255;
  }
  return exp_table_[static_cast<size_t>(mod_power)];
}

uint8_t GaloisField::log(uint8_t val) const noexcept {
  assert(val != 0 && "log(0) is mathematically undefined in GF(2^8)");
  return log_table_[val];
}

uint8_t GaloisField::poly_eval(const std::vector<uint8_t> &poly,
                               uint8_t x) const noexcept {
  if (poly.empty()) {
    return 0;
  }
  if (x == 0) {
    return poly[0];
  }
  uint8_t result = 0;
  for (auto it = poly.rbegin(); it != poly.rend(); ++it) {
    result = add(mul(result, x), *it);
  }
  return result;
}

std::vector<uint8_t>
GaloisField::poly_mul(const std::vector<uint8_t> &p1,
                      const std::vector<uint8_t> &p2) const {
  if (p1.empty() || p2.empty()) {
    return {};
  }
  std::vector<uint8_t> result(p1.size() + p2.size() - 1, 0);
  for (size_t i = 0; i < p1.size(); ++i) {
    if (p1[i] == 0)
      continue;
    for (size_t j = 0; j < p2.size(); ++j) {
      if (p2[j] == 0)
        continue;
      result[i + j] = add(result[i + j], mul(p1[i], p2[j]));
    }
  }
  return result;
}

} // namespace ccsds
