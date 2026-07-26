#include "brlab/core/step_runner.hpp"

#include <set>
#include <utility>

namespace brlab {
namespace {

void validate_steps(const std::span<const Step> steps) {
  std::set<std::string_view> names;
  for (const Step& step : steps) {
    if (step.name.empty()) {
      throw SimulationError("step name must not be empty");
    }
    if (!names.insert(step.name).second) {
      throw SimulationError("duplicate step name: " + step.name);
    }
  }
}

bool matches_failpoint(const CutPlan& plan, const std::string& failpoint) {
  return plan.failpoint.has_value() && *plan.failpoint == failpoint;
}

} // namespace

StepRunner::StepRunner(const SimTime initial_time) : now_(initial_time) {}

SimTime StepRunner::now() const noexcept { return now_; }

RunResult StepRunner::run(const std::span<const Step> steps, const CutPlan& plan) {
  validate_steps(steps);

  std::vector<TraceEntry> trace;
  trace.reserve(steps.size() * 2U);

  const auto cut_result = [&trace, this]() {
    return RunResult{RunOutcome::PowerCut, now_, std::move(trace)};
  };

  for (const Step& step : steps) {
    const SimTime start = now_;
    trace.push_back(TraceEntry{step.name, StepBoundary::Before, start});

    const bool zero_duration = step.duration == SimTime{};
    const bool absolute_before =
        plan.absolute_time.has_value() &&
        (*plan.absolute_time < start || (!zero_duration && *plan.absolute_time == start));
    if (absolute_before || matches_failpoint(plan, step.name + ".before")) {
      return cut_result();
    }

    const SimTime end = start.checked_add(step.duration);
    const bool absolute_inside =
        plan.absolute_time.has_value() && start < *plan.absolute_time && *plan.absolute_time < end;
    if (absolute_inside) {
      const SimTime elapsed{plan.absolute_time->count() - start.count()};
      if (step.on_interrupted) {
        step.on_interrupted(elapsed, step.duration);
      }
      now_ = *plan.absolute_time;
      trace.push_back(TraceEntry{step.name, StepBoundary::Interrupted, now_});
      return cut_result();
    }

    if (step.on_complete) {
      step.on_complete();
    }
    now_ = end;
    trace.push_back(TraceEntry{step.name, StepBoundary::After, now_});

    const bool absolute_after =
        plan.absolute_time.has_value() && *plan.absolute_time == end;
    if (absolute_after || matches_failpoint(plan, step.name + ".after")) {
      return cut_result();
    }
  }

  return RunResult{RunOutcome::Completed, now_, std::move(trace)};
}

std::string to_string(const StepBoundary boundary) {
  switch (boundary) {
  case StepBoundary::Before:
    return "before";
  case StepBoundary::After:
    return "after";
  case StepBoundary::Interrupted:
    return "interrupted";
  }
  throw SimulationError("invalid StepBoundary");
}

std::string to_string(const RunOutcome outcome) {
  switch (outcome) {
  case RunOutcome::Completed:
    return "completed";
  case RunOutcome::PowerCut:
    return "power_cut";
  }
  throw SimulationError("invalid RunOutcome");
}

} // namespace brlab
