#include "brlab/core/step_runner.hpp"
#include "brlab/flash/byte_flash.hpp"
#include "brlab/persistence/record_codec.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

using namespace brlab;

void print_help() {
  std::cout << "Brownout Replay Lab\n\n"
               "Usage:\n"
               "  brlab --help\n"
               "  brlab demo\n"
               "  brlab run demo\n";
}

std::uint64_t proportional_prefix(std::size_t byte_count, SimTime elapsed, SimTime total) {
  if (total.count() == 0) {
    throw SimulationError{"program duration must be positive"};
  }
  if (elapsed > total) {
    throw SimulationError{"elapsed program time exceeds total duration"};
  }
  const auto bytes = static_cast<std::uint64_t>(byte_count);
  const auto quotient_part = (bytes / total.count()) * elapsed.count();
  const auto remainder = bytes % total.count();
  if (elapsed.count() != 0 &&
      remainder > std::numeric_limits<std::uint64_t>::max() / elapsed.count()) {
    throw SimulationError{"program prefix calculation overflow"};
  }
  return quotient_part + (remainder * elapsed.count()) / total.count();
}

int run_demo() {
  constexpr std::uint64_t cut_at_us = 40;
  const FlashGeometry geometry{ByteCount{512}, ByteCount{256}, ByteCount{256}, SimTime{100},
                               SimTime{1'000}};
  ByteFlash flash{geometry};
  const LogicalRecord record{1, {'b', 'r', 'o', 'w', 'n', 'o', 'u', 't'}};
  const auto encoded = RecordCodec::encode_uncommitted(record);
  const std::span<const std::uint8_t> body{encoded.data(), RecordCodec::commit_offset()};
  const auto commit = RecordCodec::commit_bytes();

  StepRunner runner;
  const std::vector<Step> steps{
      {"flash.program.body",
       geometry.program_duration,
       [&] { static_cast<void>(flash.program(Address{0}, body)); },
       [&](SimTime elapsed, SimTime total) {
         const auto prefix = proportional_prefix(body.size(), elapsed, total);
         static_cast<void>(flash.program_prefix(Address{0}, body, ByteCount{prefix}));
       }},
      {"flash.program.commit",
       geometry.program_duration,
       [&] {
         static_cast<void>(flash.program(
             Address{static_cast<std::uint64_t>(RecordCodec::commit_offset())}, commit));
       },
       [&](SimTime elapsed, SimTime total) {
         const auto prefix = proportional_prefix(commit.size(), elapsed, total);
         static_cast<void>(flash.program_prefix(
             Address{static_cast<std::uint64_t>(RecordCodec::commit_offset())}, commit,
             ByteCount{prefix}));
       }}};
  const auto run = runner.run(steps, CutPlan{SimTime{cut_at_us}, std::nullopt});
  const auto recovered = recover_fixed(flash, Address{0});

  std::cout << "Brownout Replay Lab deterministic demo\n"
               "seed: 1 (no random decisions in this scenario)\n"
               "\n"
               "virtual timeline\n"
               "  0 us  boot 1 (cold start)\n"
               "  0 us  begin flash.program.body\n"
            << " " << run.final_time.count() << " us  POWER CUT; programmed prefix retained\n"
            << " 40 us  boot 2 (power loss)\n"
               " 40 us  recovery before new writes\n"
            << "\nrecovery: " << to_string(recovered.status)
            << "\ndetail: " << to_string(recovered.decode_error) << '\n';
  return recovered.status == RecoveryStatus::Recovered ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    print_help();
    return 0;
  }
  if (argc == 2 && std::string_view{argv[1]} == "demo") {
    return run_demo();
  }
  if (argc == 3 && std::string_view{argv[1]} == "run" &&
      std::string_view{argv[2]} == "demo") {
    return run_demo();
  }
  print_help();
  return argc == 1 ? 0 : 2;
}
