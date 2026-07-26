#include "brlab/flash/byte_flash.hpp"

#include <cstddef>
#include <limits>
#include <string>

namespace brlab {
namespace {

[[noreturn]] void throw_flash_error(FlashError code, const std::string& message) {
  throw FlashException(code, message);
}

} // namespace

FlashException::FlashException(FlashError code, const std::string& message)
    : SimulationError(message), code_(code) {}

FlashError FlashException::code() const noexcept { return code_; }

ByteFlash::ByteFlash(FlashGeometry geometry) : geometry_(geometry) {
  const auto total_size = geometry_.total_size.count();
  const auto page_size = geometry_.page_size.count();
  const auto sector_size = geometry_.sector_size.count();

  if (total_size == 0 || page_size == 0 || sector_size == 0) {
    throw_flash_error(FlashError::InvalidGeometry, "flash sizes must be nonzero");
  }
  if (geometry_.program_duration == SimTime{} || geometry_.erase_duration == SimTime{}) {
    throw_flash_error(FlashError::InvalidGeometry, "flash operation durations must be nonzero");
  }
  if (total_size % page_size != 0 || total_size % sector_size != 0 ||
      sector_size % page_size != 0) {
    throw_flash_error(FlashError::InvalidGeometry,
                      "flash total, page, and sector sizes are not evenly divisible");
  }
  if (total_size > std::numeric_limits<std::size_t>::max()) {
    throw_flash_error(FlashError::InvalidGeometry, "flash total size exceeds addressable memory");
  }

  bytes_.assign(static_cast<std::size_t>(total_size), erased_value);
}

const FlashGeometry& ByteFlash::geometry() const noexcept { return geometry_; }

std::vector<std::uint8_t> ByteFlash::read(Address address, ByteCount size) const {
  const auto index = checked_index(address, size);
  const auto count = static_cast<std::size_t>(size.count());
  return {bytes_.begin() + static_cast<std::ptrdiff_t>(index),
          bytes_.begin() + static_cast<std::ptrdiff_t>(index + count)};
}

ProgramResult ByteFlash::program(Address address, std::span<const std::uint8_t> data) {
  return program_prefix(address, data, ByteCount{data.size()});
}

ProgramResult ByteFlash::program_prefix(Address address, std::span<const std::uint8_t> data,
                                        ByteCount completed_prefix) {
  if (completed_prefix.count() > data.size()) {
    throw_flash_error(FlashError::InvalidPrefix,
                      "completed program prefix exceeds requested byte count");
  }

  validate_program(address, data);

  const auto index = checked_index(address, ByteCount{data.size()});
  const auto prefix_size = static_cast<std::size_t>(completed_prefix.count());
  for (std::size_t offset = 0; offset < prefix_size; ++offset) {
    bytes_[index + offset] &= data[offset];
  }

  return ProgramResult{
      .requested = ByteCount{data.size()},
      .programmed = completed_prefix,
      .interrupted = completed_prefix.count() < data.size(),
  };
}

const std::vector<std::uint8_t>& ByteFlash::snapshot() const noexcept { return bytes_; }

void ByteFlash::restore(std::span<const std::uint8_t> image) {
  if (image.size() != bytes_.size()) {
    throw_flash_error(FlashError::InvalidSnapshot,
                      "flash snapshot size does not match flash geometry");
  }
  std::vector<std::uint8_t> restored(image.begin(), image.end());
  bytes_.swap(restored);
}

std::size_t ByteFlash::checked_index(Address address, ByteCount size) const {
  const auto total_size = geometry_.total_size.count();
  const auto start = address.count();
  const auto count = size.count();

  if (start > total_size || count > total_size - start) {
    throw_flash_error(FlashError::OutOfBounds, "flash access is out of bounds");
  }
  return static_cast<std::size_t>(start);
}

void ByteFlash::validate_program(Address address, std::span<const std::uint8_t> data) const {
  const auto data_size = ByteCount{data.size()};
  const auto index = checked_index(address, data_size);

  if (!data.empty()) {
    const auto page_size = geometry_.page_size.count();
    const auto first_page = address.count() / page_size;
    const auto last_address = address.count() + data.size() - 1;
    const auto last_page = last_address / page_size;
    if (first_page != last_page) {
      throw_flash_error(FlashError::CrossesPageBoundary,
                        "flash program crosses a page boundary");
    }
  }

  for (std::size_t offset = 0; offset < data.size(); ++offset) {
    const auto stored = bytes_[index + offset];
    const auto requested = data[offset];
    if ((stored & requested) != requested) {
      throw_flash_error(FlashError::IllegalBitTransition,
                        "flash program attempts an illegal zero-to-one bit transition");
    }
  }
}

std::string to_string(FlashError error) {
  switch (error) {
  case FlashError::InvalidGeometry:
    return "InvalidGeometry";
  case FlashError::OutOfBounds:
    return "OutOfBounds";
  case FlashError::CrossesPageBoundary:
    return "CrossesPageBoundary";
  case FlashError::IllegalBitTransition:
    return "IllegalBitTransition";
  case FlashError::InvalidPrefix:
    return "InvalidPrefix";
  case FlashError::InvalidSnapshot:
    return "InvalidSnapshot";
  }
  return "UnknownFlashError";
}

} // namespace brlab
