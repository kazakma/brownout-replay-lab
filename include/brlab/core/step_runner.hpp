#pragma once

#include "brlab/core/types.hpp"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace brlab {

enum class StepBoundary { Before, After, Interrupted };
enum class RunOutcome { Completed, PowerCut };

struct TraceEntry {
  std::string step_name;
  StepBoundary boundary{};
  SimTime time{};

  friend bool operator==(const TraceEntry&, const TraceEntry&) = default;
};

struct Step {
  std::string name;
  SimTime duration;
  std::function<void()> on_complete;
  std::function<void(SimTime elapsed, SimTime total)> on_interrupted;
};

struct CutPlan {
  std::optional<SimTime> absolute_time;
  std::optional<std::string> failpoint;
};

struct RunResult {
  RunOutcome outcome{};
  SimTime final_time{};
  std::vector<TraceEntry> trace;

  friend bool operator==(const RunResult&, const RunResult&) = default;
};

class StepRunner {
public:
  explicit StepRunner(SimTime initial_time = SimTime{});

  [[nodiscard]] SimTime now() const noexcept;
  [[nodiscard]] RunResult run(std::span<const Step> steps, const CutPlan& plan = {});

private:
  SimTime now_;
};

[[nodiscard]] std::string to_string(StepBoundary boundary);
[[nodiscard]] std::string to_string(RunOutcome outcome);

} // namespace brlab
