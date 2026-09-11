#include "ccsds/reed_solomon.hpp"

#include <algorithm>
#include <cassert>

namespace ccsds {

ReedSolomon::ReedSolomon(int first_root) : first_root_(first_root) { compute_generator_poly(); }

void ReedSolomon::compute_generator_poly() {
  generator_poly_ = {1};
  for (int i = 0; i < 32; ++i) {
    uint8_t root = gf_.exp(first_root_ + i);
    std::vector<uint8_t> factor = {root, 1};
    generator_poly_ = gf_.poly_mul(generator_poly_, factor);
  }
}

void ReedSolomon::encode(std::span<const uint8_t> message,
                         std::span<uint8_t> output_codeword) const {
  assert(message.size() <= 223 && "Message must be <= 223 bytes");
  assert(output_codeword.size() == 255 && "Output codeword must be exactly 255 bytes");

  std::fill(output_codeword.begin(), output_codeword.end(), 0);
  std::copy(message.begin(), message.end(), output_codeword.begin());

  std::vector<uint8_t> remainder(32, 0);

  for (size_t i = 0; i < message.size(); ++i) {
    uint8_t feedback = GaloisField::add(message[i], remainder[31]);
    if (feedback != 0) {
      for (size_t j = 31; j > 0; --j) {
        remainder[j] = GaloisField::add(remainder[j - 1], gf_.mul(feedback, generator_poly_[j]));
      }
      remainder[0] = gf_.mul(feedback, generator_poly_[0]);
    } else {
      for (size_t j = 31; j > 0; --j) {
        remainder[j] = remainder[j - 1];
      }
      remainder[0] = 0;
    }
  }

  for (size_t i = 0; i < 32; ++i) {
    output_codeword[223 + i] = remainder[31 - i];
  }
}

std::vector<uint8_t> ReedSolomon::compute_syndromes(std::span<const uint8_t> codeword) const {
  std::vector<uint8_t> syndromes(32, 0);
  for (size_t i = 0; i < 32; ++i) {
    uint8_t root = gf_.exp(first_root_ + static_cast<int>(i));
    uint8_t syn = 0;
    for (uint8_t byte : codeword) {
      syn = GaloisField::add(gf_.mul(syn, root), byte);
    }
    syndromes[i] = syn;
  }
  return syndromes;
}

std::vector<uint8_t> ReedSolomon::berlekamp_massey(const std::vector<uint8_t>& syndromes) const {
  std::vector<uint8_t> lambda = {1};
  std::vector<uint8_t> b = {1};
  size_t l = 0;
  size_t m = 1;
  uint8_t b_disc = 1;

  for (size_t n = 0; n < 32; ++n) {
    uint8_t d = syndromes[n];
    for (size_t i = 1; i < lambda.size(); ++i) {
      d = GaloisField::add(d, gf_.mul(lambda[i], syndromes[n - i]));
    }

    if (d == 0) {
      ++m;
    } else {
      std::vector<uint8_t> old_lambda = lambda;
      uint8_t factor = gf_.div(d, b_disc);

      size_t needed_size = b.size() + m;
      if (lambda.size() < needed_size) {
        lambda.resize(needed_size, 0);
      }
      for (size_t i = 0; i < b.size(); ++i) {
        lambda[i + m] = GaloisField::add(lambda[i + m], gf_.mul(factor, b[i]));
      }

      if (2 * l <= n) {
        l = n + 1 - l;
        b = old_lambda;
        b_disc = d;
        m = 1;
      } else {
        ++m;
      }
    }
  }

  while (lambda.size() > 1 && lambda.back() == 0) {
    lambda.pop_back();
  }

  return lambda;
}

std::vector<uint8_t> ReedSolomon::formal_derivative(const std::vector<uint8_t>& poly) const {
  if (poly.size() <= 1) {
    return {0};
  }
  std::vector<uint8_t> deriv(poly.size() - 1, 0);
  for (size_t i = 1; i < poly.size(); ++i) {
    if ((i & 1) != 0) {
      deriv[i - 1] = poly[i];
    } else {
      deriv[i - 1] = 0;
    }
  }
  while (deriv.size() > 1 && deriv.back() == 0) {
    deriv.pop_back();
  }
  return deriv;
}

RsDecodeResult ReedSolomon::decode(std::span<uint8_t> codeword) const {
  RsDecodeResult result;
  if (codeword.size() != 255) {
    return result;
  }

  std::vector<uint8_t> syndromes = compute_syndromes(codeword);

  bool has_error = false;
  for (uint8_t s : syndromes) {
    if (s != 0) {
      has_error = true;
      break;
    }
  }
  if (!has_error) {
    result.success = true;
    result.corrected = false;
    result.errors_corrected = 0;
    return result;
  }

  std::vector<uint8_t> lambda = berlekamp_massey(syndromes);
  size_t num_errors = lambda.size() - 1;

  if (num_errors == 0 || num_errors > 16) {
    return result;
  }

  std::vector<size_t> error_locs;
  std::vector<uint8_t> root_inverses;

  for (int p = 0; p < 255; ++p) {
    uint8_t x_inv = gf_.exp(255 - p);
    if (gf_.poly_eval(lambda, x_inv) == 0) {
      error_locs.push_back(static_cast<size_t>(254 - p));
      root_inverses.push_back(x_inv);
    }
  }

  if (error_locs.size() != num_errors) {
    return result;
  }

  std::vector<uint8_t> omega = gf_.poly_mul(syndromes, lambda);
  if (omega.size() > 32) {
    omega.resize(32);
  }

  std::vector<uint8_t> lambda_prime = formal_derivative(lambda);

  for (size_t i = 0; i < num_errors; ++i) {
    size_t pos = error_locs[i];
    uint8_t x_inv = root_inverses[i];
    uint8_t x_val = gf_.inv(x_inv);

    uint8_t omega_val = gf_.poly_eval(omega, x_inv);
    uint8_t lambda_prime_val = gf_.poly_eval(lambda_prime, x_inv);

    if (lambda_prime_val == 0) {
      return result;
    }

    uint8_t scale = gf_.exp((1 - first_root_) * static_cast<int>(gf_.log(x_val)));
    uint8_t error_val = gf_.mul(scale, gf_.div(omega_val, lambda_prime_val));

    codeword[pos] = GaloisField::add(codeword[pos], error_val);
  }

  std::vector<uint8_t> check_syndromes = compute_syndromes(codeword);
  for (uint8_t s : check_syndromes) {
    if (s != 0) {
      return result;
    }
  }

  result.success = true;
  result.corrected = true;
  result.errors_corrected = num_errors;
  result.error_positions = error_locs;
  return result;
}

bool ReedSolomon::decode_interleaved(std::span<uint8_t> data, size_t interleaving_depth,
                                     size_t& total_errors_corrected) const {
  if (interleaving_depth == 0 || data.size() != interleaving_depth * 255) {
    return false;
  }

  total_errors_corrected = 0;
  std::vector<uint8_t> block(255);

  for (size_t d = 0; d < interleaving_depth; ++d) {
    for (size_t j = 0; j < 255; ++j) {
      block[j] = data[j * interleaving_depth + d];
    }

    RsDecodeResult res = decode(block);
    if (!res.success) {
      return false;
    }

    total_errors_corrected += res.errors_corrected;

    for (size_t j = 0; j < 255; ++j) {
      data[j * interleaving_depth + d] = block[j];
    }
  }

  return true;
}

}  // namespace ccsds
