#include "brlab/persistence/record_codec.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace brlab {
namespace {

constexpr std::array<std::uint8_t, 4> record_magic{'B', 'R', 'L', '1'};
constexpr std::uint32_t crc32_polynomial = 0xEDB88320U;

void write_u32_le(std::span<std::uint8_t> destination, std::size_t offset,
                  std::uint32_t value) {
  for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
    destination[offset + byte] =
        static_cast<std::uint8_t>((value >> (byte * 8U)) & 0xFFU);
  }
}

void write_u64_le(std::span<std::uint8_t> destination, std::size_t offset,
                  std::uint64_t value) {
  for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
    destination[offset + byte] =
        static_cast<std::uint8_t>((value >> (byte * 8U)) & 0xFFU);
  }
}

[[nodiscard]] std::uint32_t read_u32_le(std::span<const std::uint8_t> source,
                                        std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
    value |= static_cast<std::uint32_t>(source[offset + byte]) << (byte * 8U);
  }
  return value;
}

[[nodiscard]] std::uint64_t read_u64_le(std::span<const std::uint8_t> source,
                                        std::size_t offset) {
  std::uint64_t value = 0;
  for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
    value |= static_cast<std::uint64_t>(source[offset + byte]) << (byte * 8U);
  }
  return value;
}

[[nodiscard]] std::uint32_t crc32_iso_hdlc(std::span<const std::uint8_t> bytes) {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (const auto value : bytes) {
    crc ^= value;
    for (unsigned bit = 0; bit < 8U; ++bit) {
      const bool reflected_bit_is_set = (crc & 1U) != 0U;
      crc >>= 1U;
      if (reflected_bit_is_set) {
        crc ^= crc32_polynomial;
      }
    }
  }
  return crc ^ 0xFFFFFFFFU;
}

[[nodiscard]] DecodeResult decode_error(DecodeError error) {
  return {LogicalRecord{}, error};
}

} // namespace

std::vector<std::uint8_t> RecordCodec::encode_uncommitted(const LogicalRecord& record) {
  static_cast<void>(encoded_size(record.payload.size()));

  std::vector<std::uint8_t> encoded(slot_size, erased_commit_marker);
  std::copy(record_magic.begin(), record_magic.end(), encoded.begin());
  encoded[4] = format_version;
  write_u32_le(encoded, 5, static_cast<std::uint32_t>(record.payload.size()));
  write_u64_le(encoded, 9, record.sequence);
  std::copy(record.payload.begin(), record.payload.end(),
            encoded.begin() + static_cast<std::ptrdiff_t>(fixed_header_size));

  const auto covered_size = fixed_header_size + record.payload.size();
  const auto crc = crc32_iso_hdlc(std::span<const std::uint8_t>{encoded.data(), covered_size});
  write_u32_le(encoded, covered_size, crc);
  return encoded;
}

std::vector<std::uint8_t> RecordCodec::commit_bytes() { return {committed_marker}; }

std::size_t RecordCodec::encoded_size(std::size_t payload_size) {
  if (payload_size > maximum_payload) {
    throw SimulationError{"record payload exceeds maximum payload size"};
  }
  return slot_size;
}

DecodeResult RecordCodec::decode(std::span<const std::uint8_t> bytes) {
  if (bytes.size() != slot_size) {
    return decode_error(DecodeError::Truncated);
  }
  if (std::all_of(bytes.begin(), bytes.end(),
                  [](std::uint8_t value) { return value == erased_commit_marker; })) {
    return decode_error(DecodeError::Empty);
  }
  if (bytes[commit_offset()] != committed_marker) {
    return decode_error(DecodeError::Uncommitted);
  }

  if (!std::equal(record_magic.begin(), record_magic.end(), bytes.begin())) {
    return decode_error(DecodeError::BadMagic);
  }
  if (bytes[4] != format_version) {
    return decode_error(DecodeError::UnsupportedVersion);
  }

  const auto payload_length = read_u32_le(bytes, 5);
  if (payload_length > maximum_payload) {
    return decode_error(DecodeError::InvalidLength);
  }
  const auto payload_size = static_cast<std::size_t>(payload_length);
  if (payload_size > commit_offset() - fixed_header_size - crc_size) {
    return decode_error(DecodeError::InvalidLength);
  }

  const auto covered_size = fixed_header_size + payload_size;
  const auto stored_crc = read_u32_le(bytes, covered_size);
  const auto calculated_crc = crc32_iso_hdlc(bytes.first(covered_size));
  if (stored_crc != calculated_crc) {
    return decode_error(DecodeError::CrcMismatch);
  }

  LogicalRecord record;
  record.sequence = read_u64_le(bytes, 9);
  record.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(fixed_header_size),
                        bytes.begin() + static_cast<std::ptrdiff_t>(covered_size));
  return {std::move(record), DecodeError::None};
}

RecoveryResult recover_fixed(const ByteFlash& flash, Address address) {
  const auto bytes =
      flash.read(address, ByteCount{static_cast<std::uint64_t>(RecordCodec::slot_size)});
  const auto decoded = RecordCodec::decode(bytes);
  if (decoded.has_value()) {
    return {RecoveryStatus::Recovered, decoded.record, DecodeError::None};
  }
  if (decoded.error == DecodeError::Empty || decoded.error == DecodeError::Uncommitted) {
    return {RecoveryStatus::NoCommittedRecord, {}, decoded.error};
  }
  return {RecoveryStatus::DetectedCorruption, {}, decoded.error};
}

std::string to_string(DecodeError error) {
  switch (error) {
  case DecodeError::None:
    return "none";
  case DecodeError::Empty:
    return "empty";
  case DecodeError::BadMagic:
    return "bad magic";
  case DecodeError::UnsupportedVersion:
    return "unsupported version";
  case DecodeError::InvalidLength:
    return "invalid length";
  case DecodeError::CrcMismatch:
    return "CRC mismatch";
  case DecodeError::Uncommitted:
    return "uncommitted";
  case DecodeError::Truncated:
    return "truncated";
  }
  return "unknown decode error";
}

std::string to_string(RecoveryStatus status) {
  switch (status) {
  case RecoveryStatus::Recovered:
    return "recovered";
  case RecoveryStatus::NoCommittedRecord:
    return "no committed record";
  case RecoveryStatus::DetectedCorruption:
    return "detected corruption";
  }
  return "unknown recovery status";
}

} // namespace brlab
