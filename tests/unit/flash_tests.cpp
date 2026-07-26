#include "brlab/flash/byte_flash.hpp"
#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace {

brlab::FlashGeometry test_geometry() {
  return {
      .total_size = brlab::ByteCount{64},
      .page_size = brlab::ByteCount{8},
      .sector_size = brlab::ByteCount{16},
      .program_duration = brlab::SimTime{100},
      .erase_duration = brlab::SimTime{1'000},
  };
}

template <typename Function>
void require_flash_error(Function&& function, brlab::FlashError expected) {
  try {
    function();
  } catch (const brlab::FlashException& error) {
    BRLAB_REQUIRE(error.code() == expected);
    return;
  }
  BRLAB_REQUIRE(false);
}

} // namespace

BRLAB_TEST("flash geometry rejects zero and non-divisible sizes") {
  auto geometry = test_geometry();
  geometry.page_size = brlab::ByteCount{0};
  require_flash_error([&] { brlab::ByteFlash flash{geometry}; },
                      brlab::FlashError::InvalidGeometry);

  geometry = test_geometry();
  geometry.total_size = brlab::ByteCount{63};
  require_flash_error([&] { brlab::ByteFlash flash{geometry}; },
                      brlab::FlashError::InvalidGeometry);

  geometry = test_geometry();
  geometry.total_size = brlab::ByteCount{56};
  require_flash_error([&] { brlab::ByteFlash flash{geometry}; },
                      brlab::FlashError::InvalidGeometry);

  geometry = test_geometry();
  geometry.total_size = brlab::ByteCount{60};
  geometry.page_size = brlab::ByteCount{6};
  geometry.sector_size = brlab::ByteCount{10};
  require_flash_error([&] { brlab::ByteFlash flash{geometry}; },
                      brlab::FlashError::InvalidGeometry);

  geometry = test_geometry();
  geometry.program_duration = brlab::SimTime{};
  require_flash_error([&] { brlab::ByteFlash flash{geometry}; },
                      brlab::FlashError::InvalidGeometry);

  geometry = test_geometry();
  geometry.erase_duration = brlab::SimTime{};
  require_flash_error([&] { brlab::ByteFlash flash{geometry}; },
                      brlab::FlashError::InvalidGeometry);
}

BRLAB_TEST("erased flash is all FF and preserves geometry") {
  const auto geometry = test_geometry();
  const brlab::ByteFlash flash{geometry};

  BRLAB_REQUIRE(flash.geometry().total_size == geometry.total_size);
  BRLAB_REQUIRE(flash.snapshot().size() == geometry.total_size.count());
  for (const auto byte : flash.snapshot()) {
    BRLAB_REQUIRE(byte == brlab::ByteFlash::erased_value);
  }
}

BRLAB_TEST("flash read validates bounds and allows empty range at end") {
  const brlab::ByteFlash flash{test_geometry()};

  BRLAB_REQUIRE(flash.read(brlab::Address{64}, brlab::ByteCount{0}).empty());
  require_flash_error(
      [&] { static_cast<void>(flash.read(brlab::Address{64}, brlab::ByteCount{1})); },
      brlab::FlashError::OutOfBounds);
  require_flash_error(
      [&] { static_cast<void>(flash.read(brlab::Address{60}, brlab::ByteCount{5})); },
      brlab::FlashError::OutOfBounds);
}

BRLAB_TEST("page program supports one-to-zero transitions") {
  brlab::ByteFlash flash{test_geometry()};
  const std::array<std::uint8_t, 4> first{0xF0, 0x0F, 0xAA, 0x55};
  const std::array<std::uint8_t, 4> second{0xC0, 0x03, 0x88, 0x11};

  const auto first_result = flash.program(brlab::Address{8}, first);
  const auto second_result = flash.program(brlab::Address{8}, second);

  BRLAB_REQUIRE(first_result.requested == brlab::ByteCount{4});
  BRLAB_REQUIRE(first_result.programmed == brlab::ByteCount{4});
  BRLAB_REQUIRE(!first_result.interrupted);
  BRLAB_REQUIRE(second_result.programmed == brlab::ByteCount{4});
  BRLAB_REQUIRE(flash.read(brlab::Address{8}, brlab::ByteCount{4}) ==
                std::vector<std::uint8_t>(second.begin(), second.end()));
}

BRLAB_TEST("program rejects page crossing without mutation") {
  brlab::ByteFlash flash{test_geometry()};
  const std::array<std::uint8_t, 2> data{0x00, 0x00};
  const auto before = flash.snapshot();

  require_flash_error([&] { static_cast<void>(flash.program(brlab::Address{7}, data)); },
                      brlab::FlashError::CrossesPageBoundary);
  BRLAB_REQUIRE(flash.snapshot() == before);
}

BRLAB_TEST("program rejects out-of-bounds range without mutation") {
  brlab::ByteFlash flash{test_geometry()};
  const std::array<std::uint8_t, 2> data{0x00, 0x00};
  const auto before = flash.snapshot();

  require_flash_error([&] { static_cast<void>(flash.program(brlab::Address{64}, data)); },
                      brlab::FlashError::OutOfBounds);
  BRLAB_REQUIRE(flash.snapshot() == before);
}

BRLAB_TEST("illegal zero-to-one transition rejects the whole program atomically") {
  brlab::ByteFlash flash{test_geometry()};
  const std::array<std::uint8_t, 2> initial{0xFF, 0x00};
  static_cast<void>(flash.program(brlab::Address{0}, initial));
  const auto before = flash.snapshot();
  const std::array<std::uint8_t, 2> invalid{0x00, 0x01};

  require_flash_error([&] { static_cast<void>(flash.program(brlab::Address{0}, invalid)); },
                      brlab::FlashError::IllegalBitTransition);
  BRLAB_REQUIRE(flash.snapshot() == before);
}

BRLAB_TEST("prefix program with zero bytes is a deterministic no-op") {
  brlab::ByteFlash flash{test_geometry()};
  const std::array<std::uint8_t, 3> data{0x00, 0xAA, 0x55};
  const auto before = flash.snapshot();

  const auto result =
      flash.program_prefix(brlab::Address{2}, data, brlab::ByteCount{0});

  BRLAB_REQUIRE(result.requested == brlab::ByteCount{3});
  BRLAB_REQUIRE(result.programmed == brlab::ByteCount{0});
  BRLAB_REQUIRE(result.interrupted);
  BRLAB_REQUIRE(flash.snapshot() == before);
}

BRLAB_TEST("prefix program mutates exactly the requested middle prefix") {
  brlab::ByteFlash flash{test_geometry()};
  const std::array<std::uint8_t, 4> data{0xF0, 0x0F, 0xAA, 0x55};

  const auto result =
      flash.program_prefix(brlab::Address{8}, data, brlab::ByteCount{2});

  BRLAB_REQUIRE(result.requested == brlab::ByteCount{4});
  BRLAB_REQUIRE(result.programmed == brlab::ByteCount{2});
  BRLAB_REQUIRE(result.interrupted);
  const std::array<std::uint8_t, 4> expected{0xF0, 0x0F, 0xFF, 0xFF};
  BRLAB_REQUIRE(flash.read(brlab::Address{8}, brlab::ByteCount{4}) ==
                std::vector<std::uint8_t>(expected.begin(), expected.end()));
}

BRLAB_TEST("full prefix is equivalent to program") {
  brlab::ByteFlash prefix_flash{test_geometry()};
  brlab::ByteFlash program_flash{test_geometry()};
  const std::array<std::uint8_t, 4> data{0xF0, 0x0F, 0xAA, 0x55};

  const auto prefix_result =
      prefix_flash.program_prefix(brlab::Address{16}, data, brlab::ByteCount{data.size()});
  const auto program_result = program_flash.program(brlab::Address{16}, data);

  BRLAB_REQUIRE(!prefix_result.interrupted);
  BRLAB_REQUIRE(prefix_result.programmed == brlab::ByteCount{data.size()});
  BRLAB_REQUIRE(prefix_result.requested == program_result.requested);
  BRLAB_REQUIRE(prefix_flash.snapshot() == program_flash.snapshot());
}

BRLAB_TEST("prefix validation covers the full request before mutation") {
  brlab::ByteFlash flash{test_geometry()};
  const std::array<std::uint8_t, 3> initial{0xFF, 0xFF, 0x00};
  static_cast<void>(flash.program(brlab::Address{0}, initial));
  const auto before = flash.snapshot();
  const std::array<std::uint8_t, 3> invalid_suffix{0x00, 0x00, 0x01};

  require_flash_error(
      [&] {
        static_cast<void>(
            flash.program_prefix(brlab::Address{0}, invalid_suffix, brlab::ByteCount{2}));
      },
      brlab::FlashError::IllegalBitTransition);
  BRLAB_REQUIRE(flash.snapshot() == before);
}

BRLAB_TEST("prefix longer than data is rejected without mutation") {
  brlab::ByteFlash flash{test_geometry()};
  const std::array<std::uint8_t, 2> data{0x00, 0x00};
  const auto before = flash.snapshot();

  require_flash_error(
      [&] { static_cast<void>(flash.program_prefix(brlab::Address{0}, data, brlab::ByteCount{3})); },
      brlab::FlashError::InvalidPrefix);
  BRLAB_REQUIRE(flash.snapshot() == before);
}

BRLAB_TEST("empty program is a deterministic no-op") {
  brlab::ByteFlash flash{test_geometry()};
  const std::span<const std::uint8_t> empty;
  const auto before = flash.snapshot();

  const auto result = flash.program(brlab::Address{64}, empty);

  BRLAB_REQUIRE(result.requested == brlab::ByteCount{0});
  BRLAB_REQUIRE(result.programmed == brlab::ByteCount{0});
  BRLAB_REQUIRE(!result.interrupted);
  BRLAB_REQUIRE(flash.snapshot() == before);
}

BRLAB_TEST("snapshot restore round-trips and rejects wrong size atomically") {
  brlab::ByteFlash source{test_geometry()};
  const std::array<std::uint8_t, 3> data{0x12, 0x34, 0x56};
  static_cast<void>(source.program(brlab::Address{1}, data));
  const auto image = source.snapshot();

  brlab::ByteFlash restored{test_geometry()};
  restored.restore(image);
  BRLAB_REQUIRE(restored.snapshot() == image);
  restored.restore(restored.snapshot());
  BRLAB_REQUIRE(restored.snapshot() == image);

  const auto before_invalid_restore = restored.snapshot();
  const std::array<std::uint8_t, 2> wrong_size{0x00, 0x00};
  require_flash_error([&] { restored.restore(wrong_size); },
                      brlab::FlashError::InvalidSnapshot);
  BRLAB_REQUIRE(restored.snapshot() == before_invalid_restore);
}

BRLAB_TEST("flash errors have stable names") {
  BRLAB_REQUIRE(brlab::to_string(brlab::FlashError::InvalidGeometry) == "InvalidGeometry");
  BRLAB_REQUIRE(brlab::to_string(brlab::FlashError::OutOfBounds) == "OutOfBounds");
  BRLAB_REQUIRE(brlab::to_string(brlab::FlashError::CrossesPageBoundary) ==
                "CrossesPageBoundary");
  BRLAB_REQUIRE(brlab::to_string(brlab::FlashError::IllegalBitTransition) ==
                "IllegalBitTransition");
  BRLAB_REQUIRE(brlab::to_string(brlab::FlashError::InvalidPrefix) == "InvalidPrefix");
  BRLAB_REQUIRE(brlab::to_string(brlab::FlashError::InvalidSnapshot) == "InvalidSnapshot");
}
