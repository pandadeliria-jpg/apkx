#pragma once

#include <string>
#include <vector>

namespace apkx {

struct Args {
  std::string cmd;
  std::vector<std::string> positionals;
};

Args parse_args(int argc, char** argv);
int run_cli(const Args& args);

} // namespace apkx

