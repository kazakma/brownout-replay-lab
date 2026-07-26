#include "brlab/core/types.hpp"

#include <limits>

namespace brlab {

SimTime SimTime::checked_add(const SimTime duration) const {
  if (duration.microseconds_ > std::numeric_limits<std::uint64_t>::max() - microseconds_) {
    throw SimulationError("SimTime addition overflow");
  }
  return SimTime{microseconds_ + duration.microseconds_};
}

Address Address::checked_add(const ByteCount size) const {
  if (size.count() > std::numeric_limits<std::uint64_t>::max() - bytes_) {
    throw SimulationError("Address addition overflow");
  }
  return Address{bytes_ + size.count()};
}

} // namespace brlab
