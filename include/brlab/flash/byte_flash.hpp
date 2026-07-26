#pragma once

#include "brlab/core/types.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace brlab {

struct FlashGeometry {
  ByteCount total_size;
  ByteCount page_size;
  ByteCount sector_size;
  SimTime program_duration;
  SimTime erase_duration;
};

enum class FlashError {
  InvalidGeometry,
  OutOfBounds,
  CrossesPageBoundary,
  IllegalBitTransition,
  InvalidPrefix,
  InvalidSnapshot
};

class FlashException : public SimulationError {
public:
  FlashException(FlashError code, const std::string& message);
  [[nodiscard]] FlashError code() const noexcept;

private:
  FlashError code_;
};

struct ProgramResult {
  ByteCount requested;
  ByteCount programmed;
  bool interrupted{};
};

class ByteFlash {
public:
  static constexpr std::uint8_t erased_value = 0xFF;

  explicit ByteFlash(FlashGeometry geometry);

  [[nodiscard]] const FlashGeometry& geometry() const noexcept;
  [[nodiscard]] std::vector<std::uint8_t> read(Address address, ByteCount size) const;
  [[nodiscard]] ProgramResult program(Address address, std::span<const std::uint8_t> data);
  [[nodiscard]] ProgramResult program_prefix(Address address, std::span<const std::uint8_t> data,
                                             ByteCount completed_prefix);
  [[nodiscard]] const std::vector<std::uint8_t>& snapshot() const noexcept;
  void restore(std::span<const std::uint8_t> image);

private:
  [[nodiscard]] std::size_t checked_index(Address address, ByteCount size) const;
  void validate_program(Address address, std::span<const std::uint8_t> data) const;

  FlashGeometry geometry_;
  std::vector<std::uint8_t> bytes_;
};

[[nodiscard]] std::string to_string(FlashError error);

} // namespace brlab
