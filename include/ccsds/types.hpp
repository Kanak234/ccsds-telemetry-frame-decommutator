#ifndef CCSDS_TYPES_HPP
#define CCSDS_TYPES_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ccsds {

// Standard CCSDS 32-bit Attached Synchronization Marker (ASM) constants.
// Used by the frame synchronizer to locate Channel Access Data Unit (CADU)
// boundaries.
inline constexpr uint32_t FORWARD_ASM = 0x1ACFFC1D;
inline constexpr uint32_t INVERTED_ASM =
    0xE53003E2; // Bitwise NOT of FORWARD_ASM (180 deg phase ambiguity)

// Standard CCSDS default frame dimensions (in bytes)
inline constexpr size_t ASM_SIZE = 4;
inline constexpr size_t DEFAULT_CADU_SIZE = 1024;
inline constexpr size_t DEFAULT_TF_SIZE = DEFAULT_CADU_SIZE - ASM_SIZE;
inline constexpr size_t TF_PRIMARY_HEADER_SIZE = 6;
inline constexpr size_t SPACE_PACKET_HEADER_SIZE = 6;
inline constexpr size_t RS_CODEWORD_SIZE = 255;
inline constexpr size_t RS_DATA_SIZE = 223;
inline constexpr size_t RS_PARITY_SIZE = 32;
inline constexpr uint16_t IDLE_APID =
    0x07FF; // CCSDS 133.0-B-1 Reserved Idle Packet APID

// Lifecycle states for the CCSDS Frame Synchronization State Machine (CCSDS
// 131.0-B-3)
enum class SyncState {
  SEARCH,  // Scanning bit-by-bit or byte-by-byte for ASM correlation
  CHECK,   // Tentative ASM found; verifying next frame boundary periodicity
  LOCK,    // Frame synchronization established; passing CADUs to downstream
           // decoder
  FLYWHEEL // Frame ASM correlation lost; synthesizing boundary up to max
           // tolerance
};

// Represents a raw or synchronized Channel Access Data Unit (CADU)
struct Cadu {
  uint32_t asm_pattern{0};   // Detected 32-bit ASM
  bool inverted{false};      // True if inverted ASM was matched
  size_t bit_errors{0};      // Number of bit flips in ASM match
  std::vector<uint8_t> data; // Transfer Frame payload (without ASM)
};

// Parsed fields from a CCSDS Telemetry (TM) Transfer Frame Primary Header
// (CCSDS 132.0-B-2)
struct TransferFrameHeader {
  uint8_t version{0}; // 2 bits: Transfer Frame Version (00 = TM, 01 = AOS)
  uint16_t spacecraft_id{0};     // 10 bits: Spacecraft Identifier (SCID)
  uint8_t virtual_channel_id{0}; // 3 bits: Virtual Channel Identifier (VCID)
  bool ocf_flag{false};          // 1 bit: Operational Control Field present
  uint8_t master_frame_count{
      0}; // 8 bits: Sequential Master Channel Frame Count
  uint8_t virtual_frame_count{
      0}; // 8 bits: Sequential Virtual Channel Frame Count
  bool secondary_header_flag{
      false}; // 1 bit: Transfer Frame Secondary Header Present
  bool synch_flag{
      false}; // 1 bit: 0 = Synchronous Space Packets, 1 = Private Data
  bool packet_order_flag{false}; // 1 bit: Reserved / Packet Order
  uint8_t segment_length_id{0};  // 2 bits: Segment Length Identifier
  uint16_t first_header_pointer{
      0}; // 11 bits: Byte offset to start of first Space Packet (0x7FE = idle,
          // 0x7FF = no start)
};

// Represents a fully decommutated CCSDS Space Packet (CCSDS 133.0-B-1)
struct SpacePacket {
  uint8_t version{0}; // 3 bits: Packet Version Number (always 000)
  bool type{false};   // 1 bit: 0 = Telemetry, 1 = Telecommand
  bool secondary_header_flag{
      false};       // 1 bit: Secondary header (timestamp) present
  uint16_t apid{0}; // 11 bits: Application Process Identifier
  uint8_t sequence_flags{
      0}; // 2 bits: 00 = continuation, 01 = first, 10 = last, 11 = standalone
  uint16_t sequence_count{0}; // 14 bits: Packet Sequence Count
  uint16_t packet_data_length{
      0}; // 16 bits: Payload size minus 1 (Total payload = length + 1)
  std::vector<uint8_t> payload; // User application telemetry bytes
};

// Configuration parameters for the decommutation pipeline
struct DecommutatorConfig {
  size_t frame_size{DEFAULT_CADU_SIZE}; // Total CADU size including 4-byte ASM
  size_t asm_tolerance{1};              // Allowed bit flips in ASM match
  size_t check_frames_required{
      2}; // Consecutive frames required in CHECK to enter LOCK
  size_t flywheel_max_frames{
      3}; // Max synthesized frames in FLYWHEEL before dropping to SEARCH
  bool enable_descrambler{true};  // Apply CCSDS pseudo-random descrambling
  bool enable_reed_solomon{true}; // Apply Reed-Solomon (255, 223) FEC decoding
  bool enable_crc_check{true}; // Verify CRC-16-CCITT Frame Error Control Field
};

// Operational statistics recorded across the processing session
struct PipelineStats {
  size_t bytes_ingested{0};
  size_t cadus_synchronized{0};
  size_t asm_bit_errors{0};
  size_t rs_corrected_symbols{0};
  size_t rs_uncorrectable_frames{0};
  size_t crc_passed_frames{0};
  size_t crc_failed_frames{0};
  size_t packets_extracted{0};
  size_t packets_dropped{0};
};

} // namespace ccsds

#endif // CCSDS_TYPES_HPP
