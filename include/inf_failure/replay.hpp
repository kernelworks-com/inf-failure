#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace inf_failure {
enum class Kind { allocate, schedule, retire, reuse, consume, assertion };
enum class Verdict { same_failure, no_match, different_failure, invalid, inconclusive };
struct Event {
  std::uint64_t id{};
  Kind kind{};
  std::uint64_t allocation{}, generation{}, operation{};
  std::string state;
  std::vector<std::uint64_t> dependencies;
};
struct Trace {
  unsigned version{1};
  bool complete{true};
  std::uint64_t dropped{};
  std::string environment{"cpu-reference-v1"};
  std::vector<Event> events;
};
struct Result {
  Verdict verdict;
  std::string signature;
  std::string detail;
  std::vector<std::uint64_t> witness;
};
// The CPU fixture executes the supplied total order, validating explicit prerequisites.
// It intentionally permits reuse with an outstanding read to model a planted bug.
Result replay(const Trace&, const std::string& target = "stale-generation");
Trace read_trace(std::istream&);
void write_trace(std::ostream&, const Trace&);
Trace fixture(bool benign);
const char* name(Verdict);
} // namespace inf_failure
