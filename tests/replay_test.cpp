#include "inf_failure/replay.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace inf_failure;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
  const auto stale = fixture(false);
  const auto benign = fixture(true);
  const auto result = replay(stale);
  check(result.verdict == Verdict::same_failure, "planted stale read");
  check(result.witness == std::vector<std::uint64_t>({1,2,3,4,5}), "explicit causal witness");
  check(replay(benign).verdict == Verdict::no_match, "benign shared readers");
  for (int i = 0; i < 20; ++i) check(replay(stale).witness == result.witness, "fresh state on every replay");
  check(replay(stale, "another-invariant").verdict == Verdict::different_failure, "different signature");
  for (std::size_t i = 0; i < stale.events.size(); ++i) {
    auto t = stale; t.events.erase(t.events.begin() + static_cast<std::ptrdiff_t>(i));
    check(replay(t).verdict != Verdict::same_failure, "necessary event deletion cannot preserve target");
  }
  auto t = stale; t.complete = false;
  check(replay(t).verdict == Verdict::inconclusive, "incomplete capture");
  t = stale; t.dropped = 1;
  check(replay(t).verdict == Verdict::inconclusive, "dropped events");
  t = stale; t.events.back().id = 1;
  check(replay(t).verdict == Verdict::invalid, "duplicate IDs");
  t = stale; t.events[0].dependencies = {5};
  check(replay(t).verdict == Verdict::invalid, "forward edge");
  t = stale; t.events.back().dependencies = {2,2};
  check(replay(t).verdict == Verdict::invalid, "duplicate dependency");
  t = stale; t.events[3].generation = 3;
  check(replay(t).verdict == Verdict::invalid, "generation skip");
  t = stale; t.events.back().operation = 99;
  check(replay(t).verdict == Verdict::invalid, "unknown operation");
  t = benign; t.events[2].operation = 1;
  check(replay(t).verdict == Verdict::invalid, "operation IDs cannot alias");
  t = benign; t.events[4].operation = 1;
  check(replay(t).verdict == Verdict::invalid, "double consume rejected");
  t = stale; t.events[1].state = "wrong-prefix";
  check(replay(t).verdict == Verdict::invalid, "wrong semantic identity rejected");
  t = stale;
  for (auto& e : t.events) e.generation = std::numeric_limits<std::uint64_t>::max();
  t.events[3].generation = 1;
  check(replay(t).verdict == Verdict::invalid, "generation wrap rejected");
  t = stale; t.events[3].state = "prefix-A";
  check(replay(t).verdict == Verdict::same_failure, "same contents still require generation match");
  t = stale; t.events.push_back({6, Kind::consume, 1,1,1,"prefix-A", {5}});
  check(replay(t).verdict == Verdict::invalid, "failure cannot hide malformed tail");
  t = stale;
  t.events.insert(t.events.begin(), {99, Kind::assertion, 0,0,0,"earlier-assertion", {}});
  check(replay(t).verdict == Verdict::different_failure, "first failure controls verdict");
  t = stale; t.events.pop_back();
  check(replay(t).verdict == Verdict::inconclusive, "unfinished read");
  t = stale; t.version = 2;
  check(replay(t).verdict == Verdict::invalid, "unsupported version");
  t = stale; t.environment = "other-engine";
  check(replay(t).verdict == Verdict::invalid, "unsupported environment");
  t = stale; t.events[0].kind = static_cast<Kind>(99);
  check(replay(t).verdict == Verdict::invalid, "unknown enum");
  t = benign; t.events.push_back({7, Kind::assertion, 0,0,0,"scheduler-assert", {6}});
  check(replay(t).verdict == Verdict::different_failure, "different planted assertion");
  check(replay(t,"scheduler-assert").verdict == Verdict::same_failure, "explicit assertion target");
  t = stale;
  for (auto& e : t.events) if (e.allocation) e.allocation = 971;
  check(replay(t).verdict == Verdict::same_failure, "signature independent of physical identity");
  for (auto sample : {stale, benign}) {
    std::stringstream stream; write_trace(stream, sample);
    auto restored = read_trace(stream);
    check(replay(restored).verdict == replay(sample).verdict, "serialized round trip");
    check(replay(restored).witness == replay(sample).witness, "serialized witness");
  }
  for (const auto* bad : {"", "IFF 2 1 0 cpu-reference-v1 0\n", "IFF 1 2 0 cpu-reference-v1 0\n",
       "IFF 1 1 0 cpu-reference-v1 -1\n", "IFF 1 1 0 cpu-reference-v1 1\n",
       "IFF 1 1 0 cpu-reference-v1 0\nextra", "IFF 1 1 0 cpu-reference-v1 1\n1 unknown 1 1 0 state 0\n",
       "IFF 1 1 0 cpu-reference-v1 1\n-1 allocate 1 1 0 state 0\n"}) {
    bool rejected = false;
    try { std::istringstream in(bad); (void)read_trace(in); } catch (const std::exception&) { rejected = true; }
    check(rejected, "malformed serialization rejected");
  }
  std::cout << "reference replay checks passed\n";
}
