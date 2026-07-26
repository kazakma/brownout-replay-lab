#include "brlab/core/step_runner.hpp"
#include "brlab/flash/byte_flash.hpp"
#include "brlab/persistence/record_codec.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace brlab;

struct ScenarioResult {
  std::vector<std::uint8_t> image;
  RunResult run;
  RecoveryResult recovery;

  friend bool operator==(const ScenarioResult&, const ScenarioResult&) = default;
};

std::uint64_t prefix_for(std::size_t bytes, SimTime elapsed, SimTime total) {
  if (total.count() == 0 || elapsed > total) {
    throw SimulationError{"invalid prefix timing"};
  }
  const auto count = static_cast<std::uint64_t>(bytes);
  const auto quotient_part = (count / total.count()) * elapsed.count();
  const auto remainder = count % total.count();
  if (elapsed.count() != 0 &&
      remainder > std::numeric_limits<std::uint64_t>::max() / elapsed.count()) {
    throw SimulationError{"prefix calculation overflow"};
  }
  return quotient_part + (remainder * elapsed.count()) / total.count();
}

ScenarioResult run_scenario(const CutPlan& plan = {}) {
  const FlashGeometry geometry{ByteCount{512}, ByteCount{256}, ByteCount{256}, SimTime{100},
                               SimTime{1'000}};
  ByteFlash flash{geometry};
  const LogicalRecord logical{7, {0x10, 0x20, 0x30, 0x40}};
  const auto encoded = RecordCodec::encode_uncommitted(logical);
  const std::span<const std::uint8_t> body{encoded.data(), RecordCodec::commit_offset()};
  const auto commit = RecordCodec::commit_bytes();

  StepRunner runner;
  const std::vector<Step> steps{
      {"flash.program.body",
       geometry.program_duration,
       [&] { static_cast<void>(flash.program(Address{0}, body)); },
       [&](SimTime elapsed, SimTime total) {
         static_cast<void>(flash.program_prefix(
             Address{0}, body, ByteCount{prefix_for(body.size(), elapsed, total)}));
       }},
      {"flash.program.commit",
       geometry.program_duration,
       [&] {
         static_cast<void>(flash.program(
             Address{static_cast<std::uint64_t>(RecordCodec::commit_offset())}, commit));
       },
       [&](SimTime elapsed, SimTime total) {
         static_cast<void>(flash.program_prefix(
             Address{static_cast<std::uint64_t>(RecordCodec::commit_offset())}, commit,
             ByteCount{prefix_for(commit.size(), elapsed, total)}));
       }}};

  const auto run = runner.run(steps, plan);
  return {flash.snapshot(), run, recover_fixed(flash, Address{0})};
}

} // namespace

BRLAB_TEST("torn body is not recovered as committed") {
  const auto scenario = run_scenario(brlab::CutPlan{brlab::SimTime{40}, std::nullopt});
  BRLAB_REQUIRE(scenario.run.outcome == brlab::RunOutcome::PowerCut);
  BRLAB_REQUIRE(scenario.run.final_time == brlab::SimTime{40});
  BRLAB_REQUIRE(scenario.image.at(brlab::RecordCodec::commit_offset()) ==
                brlab::RecordCodec::erased_commit_marker);
  BRLAB_REQUIRE(scenario.recovery.status != brlab::RecoveryStatus::Recovered);
}

BRLAB_TEST("same cut produces byte-identical state and result") {
  const auto plan = brlab::CutPlan{brlab::SimTime{40}, std::nullopt};
  BRLAB_REQUIRE(run_scenario(plan) == run_scenario(plan));
}

BRLAB_TEST("clean body and final commit recover the record") {
  const auto scenario = run_scenario();
  BRLAB_REQUIRE(scenario.run.outcome == brlab::RunOutcome::Completed);
  BRLAB_REQUIRE(scenario.run.final_time == brlab::SimTime{200});
  BRLAB_REQUIRE(scenario.recovery.status == brlab::RecoveryStatus::Recovered);
  BRLAB_REQUIRE(scenario.recovery.record.sequence == 7U);
}

BRLAB_TEST("cut before commit leaves body uncommitted") {
  const auto scenario =
      run_scenario(brlab::CutPlan{std::nullopt, std::string{"flash.program.commit.before"}});
  BRLAB_REQUIRE(scenario.run.outcome == brlab::RunOutcome::PowerCut);
  BRLAB_REQUIRE(scenario.run.final_time == brlab::SimTime{100});
  BRLAB_REQUIRE(scenario.recovery.status == brlab::RecoveryStatus::NoCommittedRecord);
}

BRLAB_TEST("cut inside one-byte commit writes no marker prefix") {
  const auto scenario = run_scenario(brlab::CutPlan{brlab::SimTime{150}, std::nullopt});
  BRLAB_REQUIRE(scenario.run.outcome == brlab::RunOutcome::PowerCut);
  BRLAB_REQUIRE(scenario.image.at(brlab::RecordCodec::commit_offset()) ==
                brlab::RecordCodec::erased_commit_marker);
  BRLAB_REQUIRE(scenario.recovery.status == brlab::RecoveryStatus::NoCommittedRecord);
}

BRLAB_TEST("cut exactly at commit end preserves committed record") {
  const auto scenario = run_scenario(brlab::CutPlan{brlab::SimTime{200}, std::nullopt});
  BRLAB_REQUIRE(scenario.run.outcome == brlab::RunOutcome::PowerCut);
  BRLAB_REQUIRE(scenario.run.final_time == brlab::SimTime{200});
  BRLAB_REQUIRE(scenario.recovery.status == brlab::RecoveryStatus::Recovered);
}
