#include "brlab/core/step_runner.hpp"
#include "brlab/core/types.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

using brlab::Address;
using brlab::ByteCount;
using brlab::CutPlan;
using brlab::RunOutcome;
using brlab::SimTime;
using brlab::SimulationError;
using brlab::Step;
using brlab::StepBoundary;
using brlab::StepRunner;
using brlab::TraceEntry;

template <typename Function>
bool throws_simulation_error(Function&& function) {
  try {
    function();
  } catch (const SimulationError&) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

} // namespace

BRLAB_TEST("checked unit arithmetic preserves strong types") {
  BRLAB_REQUIRE(SimTime{11}.checked_add(SimTime{7}) == SimTime{18});
  BRLAB_REQUIRE(Address{100}.checked_add(ByteCount{23}) == Address{123});
  BRLAB_REQUIRE(SimTime{18}.count() == 18U);
  BRLAB_REQUIRE(Address{123}.count() == 123U);
  BRLAB_REQUIRE(ByteCount{23}.count() == 23U);
}

BRLAB_TEST("checked unit arithmetic rejects overflow") {
  BRLAB_REQUIRE(throws_simulation_error(
      [] { static_cast<void>(SimTime{std::numeric_limits<std::uint64_t>::max()}
                                 .checked_add(SimTime{1})); }));
  BRLAB_REQUIRE(throws_simulation_error(
      [] { static_cast<void>(Address{std::numeric_limits<std::uint64_t>::max()}
                                 .checked_add(ByteCount{1})); }));
}

BRLAB_TEST("steps complete in insertion order and advance virtual time") {
  std::vector<std::string> callbacks;
  const std::vector<Step> steps{
      {"decode", SimTime{3}, [&callbacks] { callbacks.emplace_back("decode"); }, {}},
      {"program", SimTime{5}, [&callbacks] { callbacks.emplace_back("program"); }, {}}};
  StepRunner runner{SimTime{10}};

  const auto result = runner.run(steps);

  BRLAB_REQUIRE(result.outcome == RunOutcome::Completed);
  BRLAB_REQUIRE(result.final_time == SimTime{18});
  BRLAB_REQUIRE(runner.now() == SimTime{18});
  BRLAB_REQUIRE(callbacks == std::vector<std::string>({"decode", "program"}));
  const std::vector<TraceEntry> expected{
      {"decode", StepBoundary::Before, SimTime{10}},
      {"decode", StepBoundary::After, SimTime{13}},
      {"program", StepBoundary::Before, SimTime{13}},
      {"program", StepBoundary::After, SimTime{18}}};
  BRLAB_REQUIRE(result.trace == expected);
}

BRLAB_TEST("empty operation completes without changing time") {
  StepRunner runner{SimTime{42}};
  const std::vector<Step> steps;

  const auto result = runner.run(steps);

  BRLAB_REQUIRE(result.outcome == RunOutcome::Completed);
  BRLAB_REQUIRE(result.final_time == SimTime{42});
  BRLAB_REQUIRE(result.trace.empty());
}

BRLAB_TEST("before failpoint cuts before callback and time advance") {
  bool completed = false;
  const std::vector<Step> steps{
      {"program", SimTime{9}, [&completed] { completed = true; }, {}}};
  StepRunner runner{SimTime{4}};

  const auto result = runner.run(steps, CutPlan{std::nullopt, "program.before"});

  BRLAB_REQUIRE(result.outcome == RunOutcome::PowerCut);
  BRLAB_REQUIRE(result.final_time == SimTime{4});
  BRLAB_REQUIRE(!completed);
  const std::vector<TraceEntry> expected{
      {"program", StepBoundary::Before, SimTime{4}}};
  BRLAB_REQUIRE(result.trace == expected);
}

BRLAB_TEST("after failpoint cuts after completion") {
  bool completed = false;
  const std::vector<Step> steps{
      {"program", SimTime{9}, [&completed] { completed = true; }, {}}};
  StepRunner runner{SimTime{4}};

  const auto result = runner.run(steps, CutPlan{std::nullopt, "program.after"});

  BRLAB_REQUIRE(result.outcome == RunOutcome::PowerCut);
  BRLAB_REQUIRE(result.final_time == SimTime{13});
  BRLAB_REQUIRE(completed);
  const std::vector<TraceEntry> expected{
      {"program", StepBoundary::Before, SimTime{4}},
      {"program", StepBoundary::After, SimTime{13}}};
  BRLAB_REQUIRE(result.trace == expected);
}

BRLAB_TEST("absolute cut at step start cuts before callback") {
  bool completed = false;
  const std::vector<Step> steps{
      {"program", SimTime{9}, [&completed] { completed = true; }, {}}};
  StepRunner runner{SimTime{4}};

  const auto result = runner.run(steps, CutPlan{SimTime{4}, std::nullopt});

  BRLAB_REQUIRE(result.outcome == RunOutcome::PowerCut);
  BRLAB_REQUIRE(result.final_time == SimTime{4});
  BRLAB_REQUIRE(!completed);
  const std::vector<TraceEntry> expected{
      {"program", StepBoundary::Before, SimTime{4}}};
  BRLAB_REQUIRE(result.trace == expected);
}

BRLAB_TEST("absolute cut inside step invokes interruption and advances elapsed time") {
  bool completed = false;
  SimTime observed_elapsed;
  SimTime observed_total;
  const std::vector<Step> steps{
      {"program",
       SimTime{9},
       [&completed] { completed = true; },
       [&observed_elapsed, &observed_total](const SimTime elapsed, const SimTime total) {
         observed_elapsed = elapsed;
         observed_total = total;
       }}};
  StepRunner runner{SimTime{4}};

  const auto result = runner.run(steps, CutPlan{SimTime{10}, std::nullopt});

  BRLAB_REQUIRE(result.outcome == RunOutcome::PowerCut);
  BRLAB_REQUIRE(result.final_time == SimTime{10});
  BRLAB_REQUIRE(!completed);
  BRLAB_REQUIRE(observed_elapsed == SimTime{6});
  BRLAB_REQUIRE(observed_total == SimTime{9});
  const std::vector<TraceEntry> expected{
      {"program", StepBoundary::Before, SimTime{4}},
      {"program", StepBoundary::Interrupted, SimTime{10}}};
  BRLAB_REQUIRE(result.trace == expected);
}

BRLAB_TEST("absolute cut exactly at step end completes then cuts") {
  bool completed = false;
  bool interrupted = false;
  const std::vector<Step> steps{
      {"program",
       SimTime{9},
       [&completed] { completed = true; },
       [&interrupted](SimTime, SimTime) { interrupted = true; }}};
  StepRunner runner{SimTime{4}};

  const auto result = runner.run(steps, CutPlan{SimTime{13}, std::nullopt});

  BRLAB_REQUIRE(result.outcome == RunOutcome::PowerCut);
  BRLAB_REQUIRE(result.final_time == SimTime{13});
  BRLAB_REQUIRE(completed);
  BRLAB_REQUIRE(!interrupted);
  const std::vector<TraceEntry> expected{
      {"program", StepBoundary::Before, SimTime{4}},
      {"program", StepBoundary::After, SimTime{13}}};
  BRLAB_REQUIRE(result.trace == expected);
}

BRLAB_TEST("zero duration step completes before exact-time cut") {
  bool completed = false;
  const std::vector<Step> steps{
      {"barrier", SimTime{}, [&completed] { completed = true; }, {}}};
  StepRunner runner{SimTime{7}};

  const auto result = runner.run(steps, CutPlan{SimTime{7}, std::nullopt});

  BRLAB_REQUIRE(result.outcome == RunOutcome::PowerCut);
  BRLAB_REQUIRE(result.final_time == SimTime{7});
  BRLAB_REQUIRE(completed);
  const std::vector<TraceEntry> expected{
      {"barrier", StepBoundary::Before, SimTime{7}},
      {"barrier", StepBoundary::After, SimTime{7}}};
  BRLAB_REQUIRE(result.trace == expected);
}

BRLAB_TEST("absolute cut after operation does not alter completion") {
  bool completed = false;
  const std::vector<Step> steps{
      {"program", SimTime{9}, [&completed] { completed = true; }, {}}};
  StepRunner runner{SimTime{4}};

  const auto result = runner.run(steps, CutPlan{SimTime{14}, std::nullopt});

  BRLAB_REQUIRE(result.outcome == RunOutcome::Completed);
  BRLAB_REQUIRE(result.final_time == SimTime{13});
  BRLAB_REQUIRE(completed);
}

BRLAB_TEST("overflow throws before callback and never wraps runner time") {
  bool completed = false;
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  const std::vector<Step> steps{
      {"overflow", SimTime{2}, [&completed] { completed = true; }, {}}};
  StepRunner runner{SimTime{maximum - 1U}};

  BRLAB_REQUIRE(throws_simulation_error(
      [&runner, &steps] { static_cast<void>(runner.run(steps)); }));
  BRLAB_REQUIRE(!completed);
  BRLAB_REQUIRE(runner.now() == SimTime{maximum - 1U});
}

BRLAB_TEST("before cut takes precedence over duration overflow") {
  bool completed = false;
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  const std::vector<Step> steps{
      {"overflow", SimTime{2}, [&completed] { completed = true; }, {}}};
  StepRunner runner{SimTime{maximum - 1U}};

  const auto result = runner.run(steps, CutPlan{std::nullopt, "overflow.before"});

  BRLAB_REQUIRE(result.outcome == RunOutcome::PowerCut);
  BRLAB_REQUIRE(result.final_time == SimTime{maximum - 1U});
  BRLAB_REQUIRE(!completed);
}

BRLAB_TEST("empty and duplicate step names reject the whole operation") {
  std::size_t callbacks = 0;
  const std::vector<Step> empty_name{
      {"valid", SimTime{1}, [&callbacks] { ++callbacks; }, {}},
      {"", SimTime{1}, [&callbacks] { ++callbacks; }, {}}};
  StepRunner empty_runner;
  BRLAB_REQUIRE(throws_simulation_error(
      [&empty_runner, &empty_name] { static_cast<void>(empty_runner.run(empty_name)); }));
  BRLAB_REQUIRE(callbacks == 0U);
  BRLAB_REQUIRE(empty_runner.now() == SimTime{});

  const std::vector<Step> duplicate_name{
      {"same", SimTime{1}, [&callbacks] { ++callbacks; }, {}},
      {"same", SimTime{1}, [&callbacks] { ++callbacks; }, {}}};
  StepRunner duplicate_runner;
  BRLAB_REQUIRE(throws_simulation_error(
      [&duplicate_runner, &duplicate_name] {
        static_cast<void>(duplicate_runner.run(duplicate_name));
      }));
  BRLAB_REQUIRE(callbacks == 0U);
  BRLAB_REQUIRE(duplicate_runner.now() == SimTime{});
}

BRLAB_TEST("trace and enum strings are deterministic") {
  const std::vector<Step> steps{
      {"first", SimTime{2}, {}, {}},
      {"second", SimTime{3}, {}, {}}};
  StepRunner first;
  StepRunner second;

  BRLAB_REQUIRE(first.run(steps) == second.run(steps));
  BRLAB_REQUIRE(brlab::to_string(StepBoundary::Before) == "before");
  BRLAB_REQUIRE(brlab::to_string(StepBoundary::After) == "after");
  BRLAB_REQUIRE(brlab::to_string(StepBoundary::Interrupted) == "interrupted");
  BRLAB_REQUIRE(brlab::to_string(RunOutcome::Completed) == "completed");
  BRLAB_REQUIRE(brlab::to_string(RunOutcome::PowerCut) == "power_cut");
  BRLAB_REQUIRE(throws_simulation_error(
      [] { static_cast<void>(brlab::to_string(static_cast<StepBoundary>(99))); }));
  BRLAB_REQUIRE(throws_simulation_error(
      [] { static_cast<void>(brlab::to_string(static_cast<RunOutcome>(99))); }));
}
