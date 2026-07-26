#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace brlab {

class SimulationError : public std::runtime_error {
public:
  explicit SimulationError(const std::string& message) : std::runtime_error(message) {}
};

class SimTime {
public:
  constexpr SimTime() = default;
  explicit constexpr SimTime(std::uint64_t microseconds) : microseconds_(microseconds) {}

  [[nodiscard]] constexpr std::uint64_t count() const noexcept { return microseconds_; }
  [[nodiscard]] SimTime checked_add(SimTime duration) const;

  friend constexpr bool operator==(SimTime, SimTime) = default;
  friend constexpr auto operator<=>(SimTime, SimTime) = default;

private:
  std::uint64_t microseconds_{};
};

class ByteCount {
public:
  constexpr ByteCount() = default;
  explicit constexpr ByteCount(std::uint64_t bytes) : bytes_(bytes) {}

  [[nodiscard]] constexpr std::uint64_t count() const noexcept { return bytes_; }

  friend constexpr bool operator==(ByteCount, ByteCount) = default;
  friend constexpr auto operator<=>(ByteCount, ByteCount) = default;

private:
  std::uint64_t bytes_{};
};

class Address {
public:
  constexpr Address() = default;
  explicit constexpr Address(std::uint64_t bytes) : bytes_(bytes) {}

  [[nodiscard]] constexpr std::uint64_t count() const noexcept { return bytes_; }
  [[nodiscard]] Address checked_add(ByteCount size) const;

  friend constexpr bool operator==(Address, Address) = default;
  friend constexpr auto operator<=>(Address, Address) = default;

private:
  std::uint64_t bytes_{};
};

} // namespace brlab
