#include "apkx/cli.hpp"

#include <iostream>

int main(int argc, char** argv) {
  try {
    auto args = apkx::parse_args(argc, argv);
    return apkx::run_cli(args);
  } catch (const std::exception& e) {
    std::cerr << "[apkx] fatal: " << e.what() << "\n";
    return 2;
  }
}

