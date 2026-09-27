#include "inf_failure/replay.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  using namespace inf_failure;
  try {
    if (argc == 4 && std::string(argv[1]) == "fixture") {
      const std::string which = argv[2];
      if (which != "stale" && which != "benign") throw std::invalid_argument("fixture must be stale or benign");
      if (std::filesystem::exists(argv[3])) throw std::invalid_argument("output already exists");
      std::ofstream out(argv[3]);
      if (!out) throw std::runtime_error("cannot open output");
      write_trace(out, fixture(which == "benign"));
      out.close();
      if (!out) throw std::runtime_error("cannot finish output");
      return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "replay") {
      std::ifstream in(argv[2]);
      if (!in) throw std::invalid_argument("cannot open trace");
      auto result = replay(read_trace(in), argc == 4 ? argv[3] : "stale-generation");
      std::cout << name(result.verdict) << " signature=" << result.signature << " witness=";
      for (auto id : result.witness) std::cout << id << ',';
      std::cout << " detail=" << result.detail << '\n';
      switch (result.verdict) {
        case Verdict::same_failure: return 0;
        case Verdict::no_match: return 1;
        case Verdict::different_failure: return 2;
        case Verdict::invalid: return 3;
        case Verdict::inconclusive: return 4;
      }
    }
    std::cerr << "Usage: inf-failure fixture stale|benign FILE\n"
                 "       inf-failure replay FILE [SIGNATURE]\n";
    return 3;
  } catch (const std::exception& e) {
    std::cerr << "INVALID: " << e.what() << '\n';
    return 3;
  }
}
