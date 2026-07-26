#include "brlab/flash/byte_flash.hpp"
#include "brlab/persistence/record_codec.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {

using namespace brlab;

std::vector<std::uint8_t> committed_encoding(const LogicalRecord& record) {
  auto encoded = RecordCodec::encode_uncommitted(record);
  encoded[RecordCodec::commit_offset()] = RecordCodec::committed_marker;
  return encoded;
}

ByteFlash make_flash() {
  return ByteFlash{FlashGeometry{ByteCount{512}, ByteCount{256}, ByteCount{256}, SimTime{100},
                                 SimTime{1'000}}};
}

void program_record(ByteFlash& flash, const LogicalRecord& record) {
  const auto encoded = RecordCodec::encode_uncommitted(record);
  const std::span<const std::uint8_t> body{encoded.data(), RecordCodec::commit_offset()};
  static_cast<void>(flash.program(Address{0}, body));
  const auto commit = RecordCodec::commit_bytes();
  static_cast<void>(flash.program(Address{static_cast<std::uint64_t>(RecordCodec::commit_offset())},
                                  commit));
}

} // namespace

BRLAB_TEST("empty payload has stable little-endian golden encoding and round-trips") {
  const brlab::LogicalRecord record{0x0807060504030201ULL, {}};
  const auto encoded = brlab::RecordCodec::encode_uncommitted(record);

  const std::array<std::uint8_t, 21> golden_prefix{
      0x42, 0x52, 0x4C, 0x31, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
      0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xFB, 0x10, 0xC5, 0x31};
  BRLAB_REQUIRE(encoded.size() == brlab::RecordCodec::slot_size);
  BRLAB_REQUIRE(std::equal(golden_prefix.begin(), golden_prefix.end(), encoded.begin()));
  BRLAB_REQUIRE(std::all_of(
      encoded.begin() + static_cast<std::ptrdiff_t>(golden_prefix.size()), encoded.end(),
      [](std::uint8_t byte) { return byte == brlab::RecordCodec::erased_commit_marker; }));

  auto committed = encoded;
  committed[brlab::RecordCodec::commit_offset()] = brlab::RecordCodec::committed_marker;
  const auto decoded = brlab::RecordCodec::decode(committed);
  BRLAB_REQUIRE(decoded.has_value());
  BRLAB_REQUIRE(decoded.record == record);
}

BRLAB_TEST("maximum payload and sequence value round-trip") {
  std::vector<std::uint8_t> payload(brlab::RecordCodec::maximum_payload);
  for (std::size_t index = 0; index < payload.size(); ++index) {
    payload[index] = static_cast<std::uint8_t>(index);
  }
  const brlab::LogicalRecord record{std::numeric_limits<std::uint64_t>::max(), payload};
  const auto encoded = brlab::RecordCodec::encode_uncommitted(record);
  BRLAB_REQUIRE(brlab::RecordCodec::encoded_size(0) == brlab::RecordCodec::slot_size);
  BRLAB_REQUIRE(brlab::RecordCodec::encoded_size(payload.size()) ==
                brlab::RecordCodec::slot_size);
  BRLAB_REQUIRE(encoded.size() == brlab::RecordCodec::maximum_encoded_size());
  BRLAB_REQUIRE(encoded[brlab::RecordCodec::commit_offset()] ==
                brlab::RecordCodec::erased_commit_marker);
  BRLAB_REQUIRE(brlab::RecordCodec::commit_bytes() == std::vector<std::uint8_t>{0x00});

  const auto decoded = brlab::RecordCodec::decode(committed_encoding(record));
  BRLAB_REQUIRE(decoded.has_value());
  BRLAB_REQUIRE(decoded.record == record);
}

BRLAB_TEST("payload larger than slot capacity is rejected") {
  const auto too_large = static_cast<std::size_t>(brlab::RecordCodec::maximum_payload) + 1U;
  BRLAB_REQUIRE_THROWS(brlab::RecordCodec::encoded_size(too_large));
  BRLAB_REQUIRE_THROWS(
      brlab::RecordCodec::encode_uncommitted(brlab::LogicalRecord{1, std::vector<std::uint8_t>(
                                                                         too_large, 0xA5)}));
}

BRLAB_TEST("decode requires exactly one complete slot") {
  const auto encoded = committed_encoding(brlab::LogicalRecord{2, {0x10}});
  BRLAB_REQUIRE(brlab::RecordCodec::decode(
                    std::span<const std::uint8_t>{encoded.data(), encoded.size() - 1U})
                    .error == brlab::DecodeError::Truncated);

  auto oversized = encoded;
  oversized.push_back(0xFF);
  BRLAB_REQUIRE(brlab::RecordCodec::decode(oversized).error == brlab::DecodeError::Truncated);
}

BRLAB_TEST("empty and arbitrary torn slots are never accepted") {
  const std::vector<std::uint8_t> empty(brlab::RecordCodec::slot_size, 0xFF);
  BRLAB_REQUIRE(brlab::RecordCodec::decode(empty).error == brlab::DecodeError::Empty);

  std::vector<std::uint8_t> torn(brlab::RecordCodec::slot_size, 0x3C);
  torn[brlab::RecordCodec::commit_offset()] = brlab::RecordCodec::erased_commit_marker;
  BRLAB_REQUIRE(brlab::RecordCodec::decode(torn).error == brlab::DecodeError::Uncommitted);

  torn[brlab::RecordCodec::commit_offset()] = 0x7F;
  BRLAB_REQUIRE(brlab::RecordCodec::decode(torn).error == brlab::DecodeError::Uncommitted);
}

BRLAB_TEST("committed malformed header reports the first structural error") {
  const brlab::LogicalRecord record{3, {0x01, 0x02}};

  auto bad_magic = committed_encoding(record);
  bad_magic[0] ^= 0x01;
  BRLAB_REQUIRE(brlab::RecordCodec::decode(bad_magic).error == brlab::DecodeError::BadMagic);

  auto bad_version = committed_encoding(record);
  bad_version[4] = static_cast<std::uint8_t>(brlab::RecordCodec::format_version + 1U);
  BRLAB_REQUIRE(brlab::RecordCodec::decode(bad_version).error ==
                brlab::DecodeError::UnsupportedVersion);

  auto bad_length = committed_encoding(record);
  const auto invalid_length = brlab::RecordCodec::maximum_payload + 1U;
  bad_length[5] = static_cast<std::uint8_t>(invalid_length & 0xFFU);
  bad_length[6] = static_cast<std::uint8_t>((invalid_length >> 8U) & 0xFFU);
  bad_length[7] = static_cast<std::uint8_t>((invalid_length >> 16U) & 0xFFU);
  bad_length[8] = static_cast<std::uint8_t>((invalid_length >> 24U) & 0xFFU);
  BRLAB_REQUIRE(brlab::RecordCodec::decode(bad_length).error ==
                brlab::DecodeError::InvalidLength);
}

BRLAB_TEST("committed payload or CRC corruption is detected") {
  const brlab::LogicalRecord record{4, {0x11, 0x22, 0x33}};

  auto bad_payload = committed_encoding(record);
  bad_payload[brlab::RecordCodec::fixed_header_size + 1U] ^= 0x01;
  BRLAB_REQUIRE(brlab::RecordCodec::decode(bad_payload).error ==
                brlab::DecodeError::CrcMismatch);

  auto bad_crc = committed_encoding(record);
  const auto crc_offset = brlab::RecordCodec::fixed_header_size + record.payload.size();
  bad_crc[crc_offset] ^= 0x01;
  BRLAB_REQUIRE(brlab::RecordCodec::decode(bad_crc).error ==
                brlab::DecodeError::CrcMismatch);
}

BRLAB_TEST("recovery maps slot states and is idempotent") {
  auto empty_flash = make_flash();
  const auto empty_result = brlab::recover_fixed(empty_flash, brlab::Address{0});
  BRLAB_REQUIRE(empty_result.status == brlab::RecoveryStatus::NoCommittedRecord);
  BRLAB_REQUIRE(empty_result.decode_error == brlab::DecodeError::Empty);

  auto uncommitted_flash = make_flash();
  const auto record = brlab::LogicalRecord{9, {0xCA, 0xFE}};
  const auto uncommitted = brlab::RecordCodec::encode_uncommitted(record);
  static_cast<void>(uncommitted_flash.program(
      brlab::Address{0},
      std::span<const std::uint8_t>{uncommitted.data(), brlab::RecordCodec::commit_offset()}));
  const auto uncommitted_result = brlab::recover_fixed(uncommitted_flash, brlab::Address{0});
  BRLAB_REQUIRE(uncommitted_result.status == brlab::RecoveryStatus::NoCommittedRecord);
  BRLAB_REQUIRE(uncommitted_result.decode_error == brlab::DecodeError::Uncommitted);

  auto committed_flash = make_flash();
  program_record(committed_flash, record);
  const auto first = brlab::recover_fixed(committed_flash, brlab::Address{0});
  const auto second = brlab::recover_fixed(committed_flash, brlab::Address{0});
  BRLAB_REQUIRE(first == second);
  BRLAB_REQUIRE(first.status == brlab::RecoveryStatus::Recovered);
  BRLAB_REQUIRE(first.record == record);
}

BRLAB_TEST("recovery classifies committed corruption") {
  auto flash = make_flash();
  auto bytes = committed_encoding(brlab::LogicalRecord{10, {0x55}});
  bytes[0] ^= 0x01;
  static_cast<void>(flash.program(brlab::Address{0}, bytes));

  const auto result = brlab::recover_fixed(flash, brlab::Address{0});
  BRLAB_REQUIRE(result.status == brlab::RecoveryStatus::DetectedCorruption);
  BRLAB_REQUIRE(result.decode_error == brlab::DecodeError::BadMagic);
}
