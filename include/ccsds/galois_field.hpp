#ifndef CCSDS_GALOIS_FIELD_HPP
#define CCSDS_GALOIS_FIELD_HPP

#include <array>
#include <cstdint>
#include <vector>

namespace ccsds {

/**
 * Implements finite field arithmetic over GF(2^8) for CCSDS Reed-Solomon
 * decoding. Primitive field generator polynomial: p(x) = x^8 + x^7 + x^2 + x +
 * 1 (0x187) Generator root: alpha = 0x02
 */
class GaloisField {
public:
  GaloisField();

  // Field addition and subtraction in GF(2^8) are bitwise XOR
  [[nodiscard]] static constexpr uint8_t add(uint8_t a, uint8_t b) noexcept {
    return a ^ b;
  }

  [[nodiscard]] static constexpr uint8_t sub(uint8_t a, uint8_t b) noexcept {
    return a ^ b;
  }

  // Field multiplication via logarithmic lookup tables
  [[nodiscard]] uint8_t mul(uint8_t a, uint8_t b) const noexcept;

  // Field division: a / b
  [[nodiscard]] uint8_t div(uint8_t a, uint8_t b) const noexcept;

  // Multiplicative inverse: a^(-1) such that a * a^(-1) = 1
  [[nodiscard]] uint8_t inv(uint8_t a) const noexcept;

  // Exponentiation: alpha^power
  [[nodiscard]] uint8_t exp(int power) const noexcept;

  // Discrete logarithm: log_alpha(val)
  [[nodiscard]] uint8_t log(uint8_t val) const noexcept;

  // Evaluates a polynomial P(x) at point x using Horner's method
  // coefficients are ordered [c0, c1, c2, ...] where P(x) = c0 + c1*x + c2*x^2
  // + ...
  [[nodiscard]] uint8_t poly_eval(const std::vector<uint8_t> &poly,
                                  uint8_t x) const noexcept;

  // Polynomial multiplication in GF(2^8)[x]
  [[nodiscard]] std::vector<uint8_t>
  poly_mul(const std::vector<uint8_t> &p1,
           const std::vector<uint8_t> &p2) const;

private:
  std::array<uint8_t, 512>
      exp_table_{}; // Sized 512 to avoid modulo 255 operations in mul
  std::array<uint8_t, 256> log_table_{};

  void init_tables();
};

} // namespace ccsds

#endif // CCSDS_GALOIS_FIELD_HPP
