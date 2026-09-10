#ifndef CCSDS_REED_SOLOMON_HPP
#define CCSDS_REED_SOLOMON_HPP

#include "ccsds/galois_field.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace ccsds {

/**
 * Result structure returned after decoding an RS(255, 223) codeword.
 */
struct RsDecodeResult {
  bool success{false};   // True if codeword is clean or successfully corrected
  bool corrected{false}; // True if errors were detected and fixed
  size_t errors_corrected{0}; // Number of byte symbols corrected (0 to 16)
  std::vector<size_t> error_positions; // Indices where errors were found
};

/**
 * Implements Reed-Solomon (255, 223) Error Detection and Correction.
 * Follows CCSDS 131.0-B-3 specifications:
 * - Block Length n = 255 symbols (bytes)
 * - Information Length k = 223 symbols
 * - Parity Check Length 2t = 32 symbols
 * - Error Correction Capability t = 16 symbols
 */
class ReedSolomon {
public:
  // first_root specifies the starting power of alpha (default 112 for CCSDS TM
  // dual-basis, 1 for conventional)
  explicit ReedSolomon(int first_root = 112);

  // Encodes a 223-byte message block into a 255-byte codeword by appending 32
  // parity bytes.
  void encode(std::span<const uint8_t> message,
              std::span<uint8_t> output_codeword) const;

  // Decodes a 255-byte codeword in-place, correcting up to 16 symbol errors.
  RsDecodeResult decode(std::span<uint8_t> codeword) const;

  // De-interleaves a multi-block CADU (depth I = 1..5) and decodes each block.
  // If all blocks are correctable, writes corrected data and returns true.
  bool decode_interleaved(std::span<uint8_t> data, size_t interleaving_depth,
                          size_t &total_errors_corrected) const;

  const GaloisField &gf() const { return gf_; }

private:
  GaloisField gf_;
  int first_root_{112};
  std::vector<uint8_t> generator_poly_;

  // Computes the RS generator polynomial g(x) = product_{i=0}^{31} (x -
  // alpha^(first_root + i))
  void compute_generator_poly();

  // Computes 32 syndromes S_0 .. S_31 for the received codeword
  std::vector<uint8_t>
  compute_syndromes(std::span<const uint8_t> codeword) const;

  // Executes the Berlekamp-Massey algorithm to find the error locator
  // polynomial Lambda(x)
  std::vector<uint8_t>
  berlekamp_massey(const std::vector<uint8_t> &syndromes) const;

  // Evaluates the formal derivative of a polynomial Lambda'(x) in GF(2^8)
  std::vector<uint8_t>
  formal_derivative(const std::vector<uint8_t> &poly) const;
};

} // namespace ccsds

#endif // CCSDS_REED_SOLOMON_HPP
