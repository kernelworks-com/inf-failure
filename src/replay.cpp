#include "inf_failure/replay.hpp"

#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace inf_failure {
namespace {
const char* kind_name(Kind k) {
  switch (k) {
    case Kind::allocate: return "allocate";
    case Kind::schedule: return "schedule";
    case Kind::retire: return "retire";
    case Kind::reuse: return "reuse";
    case Kind::consume: return "consume";
    case Kind::assertion: return "assertion";
  }
  throw std::invalid_argument("unknown event kind");
}
Kind parse_kind(const std::string& s) {
  for (auto k : {Kind::allocate, Kind::schedule, Kind::retire, Kind::reuse,
                 Kind::consume, Kind::assertion}) if (s == kind_name(k)) return k;
  throw std::invalid_argument("unknown event kind: " + s);
}
bool atom(const std::string& s) {
  return !s.empty() && s.find_first_of(" \t\r\n") == std::string::npos;
}
std::uint64_t number(std::istream& in) {
  std::string s;
  if (!(in >> s) || s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
    throw std::invalid_argument("expected unsigned integer");
  std::size_t used = 0;
  auto value = std::stoull(s, &used);
  if (used != s.size()) throw std::invalid_argument("invalid unsigned integer");
  return value;
}
void end(std::istream& in) {
  std::string extra;
  if (in >> extra) throw std::invalid_argument("unexpected trailing field");
}
} // namespace

const char* name(Verdict v) {
  switch (v) {
    case Verdict::same_failure: return "SAME_FAILURE";
    case Verdict::no_match: return "NO_MATCH";
    case Verdict::different_failure: return "DIFFERENT_FAILURE";
    case Verdict::invalid: return "INVALID";
    case Verdict::inconclusive: return "INCONCLUSIVE";
  }
  return "INVALID";
}

Result replay(const Trace& t, const std::string& target) {
  auto invalid = [](std::string why) { return Result{Verdict::invalid, {}, std::move(why), {}}; };
  if (t.version != 1 || t.environment != "cpu-reference-v1") return invalid("unsupported schema or environment");
  if (!atom(target)) return invalid("empty or malformed target signature");
  std::set<std::uint64_t> seen;
  for (const auto& e : t.events) {
    if (e.id == 0 || seen.contains(e.id) || !atom(e.state)) return invalid("invalid event identity or state");
    try { (void)kind_name(e.kind); } catch (...) { return invalid("unknown event kind"); }
    std::set<std::uint64_t> deps;
    for (auto dep : e.dependencies)
      if (!seen.contains(dep) || !deps.insert(dep).second) return invalid("missing, forward, or duplicate dependency");
    seen.insert(e.id);
  }
  if (!t.complete || t.dropped) return {Verdict::inconclusive, {}, "recording is incomplete", {}};
  struct Allocation { std::uint64_t generation, origin, retirement; std::string state; bool retired; };
  struct Read { std::uint64_t allocation, generation, schedule, origin; std::string state; };
  std::map<std::uint64_t, Allocation> allocations;
  std::map<std::uint64_t, Read> reads;
  std::set<std::uint64_t> used_operations;
  Result result{Verdict::no_match, {}, "reference replay completed", {}};
  auto failure = [&](std::string signature, std::vector<std::uint64_t> witness) {
    if (result.verdict == Verdict::no_match)
      result = {signature == target ? Verdict::same_failure : Verdict::different_failure,
                std::move(signature), "observed reference invariant violation", std::move(witness)};
  };
  for (const auto& e : t.events) {
    if (e.kind == Kind::assertion) {
      if (e.allocation || e.generation || e.operation) return invalid("assertion has irrelevant identity fields");
      failure(e.state, {e.id});
      continue;
    }
    if (!e.allocation || !e.generation) return invalid("zero allocation or generation");
    auto a = allocations.find(e.allocation);
    if (e.kind == Kind::allocate) {
      if (e.operation || a != allocations.end()) return invalid("duplicate allocation or unexpected operation");
      allocations.emplace(e.allocation, Allocation{e.generation, e.id, 0, e.state, false});
      continue;
    }
    if (a == allocations.end()) return invalid("allocation prerequisite missing");
    auto& allocation = a->second;
    if (e.kind == Kind::reuse) {
      if (e.operation || !allocation.retired || allocation.generation == std::numeric_limits<std::uint64_t>::max() ||
          e.generation != allocation.generation + 1) return invalid("reuse requires retirement and next generation");
      allocation = {e.generation, e.id, allocation.retirement, e.state, false};
    } else if (e.kind == Kind::retire) {
      if (e.operation || allocation.retired || e.generation != allocation.generation || e.state != allocation.state)
        return invalid("invalid retirement");
      allocation.retired = true;
      allocation.retirement = e.id;
    } else if (e.kind == Kind::schedule) {
      if (!e.operation || used_operations.contains(e.operation) || allocation.retired ||
          e.generation != allocation.generation || e.state != allocation.state) return invalid("invalid read scheduling");
      used_operations.insert(e.operation);
      reads.emplace(e.operation, Read{e.allocation, e.generation, e.id, allocation.origin, e.state});
    } else if (e.kind == Kind::consume) {
      auto r = reads.find(e.operation);
      if (r == reads.end() || r->second.allocation != e.allocation || r->second.generation != e.generation ||
          r->second.state != e.state) return invalid("read prerequisite missing or mismatched");
      if (allocation.generation != r->second.generation || allocation.state != r->second.state)
        failure("stale-generation", {r->second.origin, r->second.schedule, allocation.retirement, allocation.origin, e.id});
      reads.erase(r);
    }
  }
  if (!reads.empty()) return {Verdict::inconclusive, {}, "outstanding reads at end of recording", {}};
  return result;
}

Trace read_trace(std::istream& in) {
  Trace t;
  std::string line, magic;
  if (!std::getline(in, line)) throw std::invalid_argument("missing header");
  std::istringstream header(line);
  header >> magic;
  const auto version = number(header), complete = number(header);
  t.dropped = number(header);
  if (!(header >> t.environment) || magic != "IFF" || version != 1 || complete > 1)
    throw std::invalid_argument("invalid trace header");
  t.version = static_cast<unsigned>(version);
  t.complete = complete == 1;
  const auto count = number(header);
  if (count > 1000000) throw std::invalid_argument("trace event limit exceeded");
  end(header);
  for (std::uint64_t i = 0; i < count; ++i) {
    if (!std::getline(in, line)) throw std::invalid_argument("truncated trace");
    std::istringstream row(line);
    Event e;
    std::string kind;
    e.id = number(row);
    if (!(row >> kind)) throw std::invalid_argument("missing kind");
    e.kind = parse_kind(kind);
    e.allocation = number(row); e.generation = number(row); e.operation = number(row);
    if (!(row >> e.state)) throw std::invalid_argument("missing state");
    const auto n = number(row);
    if (n > count) throw std::invalid_argument("dependency limit exceeded");
    for (std::uint64_t j = 0; j < n; ++j) e.dependencies.push_back(number(row));
    end(row);
    t.events.push_back(std::move(e));
  }
  end(in);
  if (in.bad()) throw std::invalid_argument("trace read failure");
  return t;
}

void write_trace(std::ostream& out, const Trace& t) {
  if (replay(t).verdict == Verdict::invalid) throw std::invalid_argument("cannot serialize invalid trace");
  out << "IFF " << t.version << ' ' << t.complete << ' ' << t.dropped << ' ' << t.environment << ' ' << t.events.size() << '\n';
  for (const auto& e : t.events) {
    out << e.id << ' ' << kind_name(e.kind) << ' ' << e.allocation << ' ' << e.generation << ' '
        << e.operation << ' ' << e.state << ' ' << e.dependencies.size();
    for (auto d : e.dependencies) out << ' ' << d;
    out << '\n';
  }
  if (!out) throw std::runtime_error("trace write failure");
}

Trace fixture(bool benign) {
  Trace t;
  t.events = {{1, Kind::allocate, 1, 1, 0, "prefix-A", {}},
              {2, Kind::schedule, 1, 1, 1, "prefix-A", {1}}};
  if (benign) {
    t.events.push_back({3, Kind::schedule, 1, 1, 2, "prefix-A", {1}});
    t.events.push_back({4, Kind::consume, 1, 1, 1, "prefix-A", {2}});
    t.events.push_back({5, Kind::consume, 1, 1, 2, "prefix-A", {3}});
    t.events.push_back({6, Kind::retire, 1, 1, 0, "prefix-A", {4, 5}});
  } else {
    t.events.push_back({3, Kind::retire, 1, 1, 0, "prefix-A", {2}});
    t.events.push_back({4, Kind::reuse, 1, 2, 0, "prefix-B", {3}});
    t.events.push_back({5, Kind::consume, 1, 1, 1, "prefix-A", {2, 4}});
  }
  return t;
}
} // namespace inf_failure
