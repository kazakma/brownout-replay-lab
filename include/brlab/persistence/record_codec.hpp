#pragma once

#include "brlab/core/types.hpp"
#include "brlab/flash/byte_flash.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace brlab {

struct LogicalRecord {
  std::uint64_t sequence{};
  std::vector<std::uint8_t> payload;

  friend bool operator==(const LogicalRecord&, const LogicalRecord&) = default;
};

enum class DecodeError {
  None,
  Empty,
  BadMagic,
  UnsupportedVersion,
  InvalidLength,
  CrcMismatch,
  Uncommitted,
  Truncated
};

struct DecodeResult {
  LogicalRecord record;
  DecodeError error{DecodeError::None};

  [[nodiscard]] bool has_value() const noexcept { return error == DecodeError::None; }
};

enum class RecoveryStatus { Recovered, NoCommittedRecord, DetectedCorruption };

struct RecoveryResult {
  RecoveryStatus status{};
  LogicalRecord record;
  DecodeError decode_error{DecodeError::None};

  friend bool operator==(const RecoveryResult&, const RecoveryResult&) = default;
};

class RecordCodec {
public:
  static constexpr std::uint8_t format_version = 1;
  static constexpr std::uint8_t erased_commit_marker = 0xFF;
  static constexpr std::uint8_t committed_marker = 0x00;
  static constexpr std::uint32_t maximum_payload = 224;
  static constexpr std::size_t fixed_header_size = 17;
  static constexpr std::size_t crc_size = 4;
  static constexpr std::size_t commit_size = 1;
  static constexpr std::size_t slot_size =
      fixed_header_size + maximum_payload + crc_size + commit_size;

  [[nodiscard]] static std::vector<std::uint8_t> encode_uncommitted(const LogicalRecord& record);
  [[nodiscard]] static std::vector<std::uint8_t> commit_bytes();
  [[nodiscard]] static constexpr std::size_t commit_offset() { return slot_size - commit_size; }
  [[nodiscard]] static std::size_t encoded_size(std::size_t payload_size);
  [[nodiscard]] static constexpr std::size_t maximum_encoded_size() { return slot_size; }
  [[nodiscard]] static DecodeResult decode(std::span<const std::uint8_t> bytes);
};

[[nodiscard]] RecoveryResult recover_fixed(const ByteFlash& flash, Address address);
[[nodiscard]] std::string to_string(DecodeError error);
[[nodiscard]] std::string to_string(RecoveryStatus status);

} // namespace brlab
