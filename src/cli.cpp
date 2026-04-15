// APK manager CLI
#include "apkx/cli.hpp"
#include "apkx/fetch.hpp"
#include "apkx/install.hpp"
#include "apkx/paths.hpp"
#include "apkx/registry.hpp"
#include "apkx/run.hpp"
#include "apkx/elf_loader.hpp"
#include "apkx/dex_loader.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

namespace apkx {

static void usage() {
  std::cerr
      << "apkx - APK manager\n\n"
      << "Usage:\n"
      << "  apkx add <package>      # Install app from Play Store (via apkeep)\n"
      << "  apkx install <path.apk>\n"
      << "  apkx fetch <url> [dest.apk]\n"
      << "  apkx list\n"
      << "  apkx info <package>\n"
      << "  apkx run <package>\n"
      << "  apkx uninstall <package>\n"
      << "  apkx analyze <path.(apk|dex|so)>\n"
      << "  apkx update apkx         # Update apkx command to latest version\n"
      << "  apkx update apps        # Update all installed apps\n"
      << "  apkx update apps <pkg>  # Update specific app\n";
}

Args parse_args(int argc, char** argv) {
  Args a;
  if (argc < 2) return a;
  a.cmd = argv[1];
  for (int i = 2; i < argc; i++) a.positionals.emplace_back(argv[i]);
  return a;
}

int run_cli(const Args& args) {
  if (args.cmd.empty()) {
    usage();
    return 1;
  }

  auto paths = default_paths();
  std::filesystem::create_directories(paths.installed_dir);
  std::filesystem::create_directories(paths.data_dir);
  std::filesystem::create_directories(paths.cache_dir);

  auto reg = load_registry(paths.registry_json);

  if (args.cmd == "list") {
    print_list(reg);
    return 0;
  }

  if (args.cmd == "info") {
    if (args.positionals.size() != 1) throw std::runtime_error("info requires <package>");
    print_info(reg, args.positionals[0]);
    return 0;
  }

  if (args.cmd == "install") {
    if (args.positionals.size() != 1) throw std::runtime_error("install requires <apk_path>");
    auto app = install_apk(paths, reg, std::filesystem::path(args.positionals[0]));
    reg.apps[app.package_name] = app;
    save_registry(paths.registry_json, reg);
    std::cout << "[apkx] installed " << app.package_name << "\n";
    return 0;
  }

  if (args.cmd == "fetch") {
    if (args.positionals.empty()) throw std::runtime_error("fetch requires <url> [dest.apk]");
    std::string url = args.positionals[0];
    std::filesystem::path dest;
    if (args.positionals.size() >= 2) {
      dest = args.positionals[1];
    } else {
      dest = std::filesystem::current_path() / "downloaded.apk";
    }
    fetch_to_file(url, dest);
    std::cout << "[apkx] downloaded to " << dest << "\n";
    try {
      auto app = install_apk(paths, reg, dest);
      reg.apps[app.package_name] = app;
      save_registry(paths.registry_json, reg);
      std::cout << "[apkx] installed " << app.package_name << "\n";
    } catch (const std::exception& e) {
      std::cerr << "[apkx] download ok, install failed: " << e.what() << "\n";
      return 1;
    }
    return 0;
  }

  if (args.cmd == "add") {
    // Install APK using apkeep
    if (args.positionals.size() != 1) throw std::runtime_error("add requires <package>");
    std::string package = args.positionals[0];
    
    std::cerr << "[apkx] Downloading " << package << " with apkeep...\n";
    
    auto cache_dir = std::filesystem::path(std::getenv("HOME")) / ".android_compat" / "cache";
    std::filesystem::create_directories(cache_dir);
    
    // Use apkeep to download
    std::string cmd = "~/.cargo/bin/apkeep -a " + package + " -d apk-pure " + cache_dir.string();
    int rc = system(cmd.c_str());
    if (rc != 0) {
        std::cerr << "[apkx] apkeep failed: " << rc << "\n";
        return 1;
    }
    
    // Find downloaded APK (apkeep may create .apk or .xapk file)
    std::filesystem::path apk_path;
    auto apk_apath = cache_dir / (package + ".apk");
    auto xapk_path = cache_dir / (package + ".xapk");
    
    if (std::filesystem::exists(apk_apath)) {
        apk_path = apk_apath;
    } else if (std::filesystem::exists(xapk_path)) {
        apk_path = xapk_path;
    } else {
        std::cerr << "[apkx] Download failed, file not found: " << package << ".apk or .xapk\n";
        return 1;
    }
    
    std::cerr << "[apkx] Installing " << apk_path << "...\n";
    auto app = install_apk(paths, reg, apk_path);
    reg.apps[app.package_name] = app;
    save_registry(paths.registry_json, reg);
    std::cerr << "[apkx] Installed: " << app.package_name << "\n";
    return 0;
  }

  if (args.cmd == "run") {
    if (args.positionals.size() != 1) throw std::runtime_error("run requires <package>");
    int rc = run_package(paths, reg, args.positionals[0]);
    save_registry(paths.registry_json, reg);
    return rc;
  }

  if (args.cmd == "uninstall") {
    if (args.positionals.size() != 1) throw std::runtime_error("uninstall requires <package>");
    int rc = uninstall_package(paths, reg, args.positionals[0]);
    save_registry(paths.registry_json, reg);
    return rc;
  }

  if (args.cmd == "analyze") {
    if (args.positionals.size() != 1) throw std::runtime_error("analyze requires <file>");
    std::filesystem::path p = args.positionals[0];
    if (!std::filesystem::exists(p)) throw std::runtime_error("file not found: " + p.string());
    
    auto ext = p.extension().string();
    if (ext == ".so" || ext == ".elf") {
      std::cout << "[apkx] Analyzing ELF: " << p << "\n";
      ElfLoader elf;
      if (elf.load(p.string())) {
        elf.dump();
      }
      return 0;
    }
    
    if (ext == ".dex") {
      std::cout << "[apkx] Analyzing DEX: " << p << "\n";
      DexLoader dex;
      if (dex.load(p.string())) {
        std::cout << "\n  Classes: " << dex.getClassCount() << "\n";
        std::cout << "  Methods: " << dex.getMethodCount() << "\n";
      }
      return 0;
    }
    
    std::cout << "[apkx] Analyzing APK: " << p << "\n";
    DexLoader dex;
    if (dex.load(p.string())) {
std::cout << "\n  Classes: " << dex.getClassCount() << "\n";
        std::cout << "  Methods: " << dex.getMethodCount() << "\n";
      }
      return 0;
    }
    
    if (args.cmd == "update") {
      if (args.positionals.empty()) {
        std::cerr << "[apkx] Usage: apkx update apkx | apps [package]\n";
        return 1;
      }
      
      std::string subcmd = args.positionals[0];
      
      // apkx update apkx
      if (subcmd == "apkx") {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
        std::string repo_dir = home + "/.apkx";
        std::string install_dir = home + "/.local/bin";
        
        if (!std::filesystem::exists(repo_dir)) {
          std::cerr << "[apkx] Cloning repository...\n";
          std::string clone_cmd = "git clone https://github.com/pandadeliria-jpg/apkx.git " + repo_dir;
          int rc = system(clone_cmd.c_str());
          if (rc != 0) {
            std::cerr << "[apkx] Clone failed.\n";
            return 1;
          }
        } else {
          std::cerr << "[apkx] Checking for apkx updates...\n";
          std::string clean_cmd = "cd " + repo_dir + " && git checkout HEAD -- . && git clean -fd";
          system(clean_cmd.c_str());
          
          std::string fetch_cmd = "cd " + repo_dir + " && git fetch origin main";
          system(fetch_cmd.c_str());
          
          char local_buf[64] = {0}, remote_buf[64] = {0};
          FILE* lf = popen(("cd " + repo_dir + " && git rev-parse HEAD").c_str(), "r");
          if (fgets(local_buf, sizeof(local_buf), lf)) {}
          pclose(lf);
          
          FILE* rf = popen(("cd " + repo_dir + " && git rev-parse origin/main").c_str(), "r");
          if (fgets(remote_buf, sizeof(remote_buf), rf)) {}
          pclose(rf);
          
          if (std::string(local_buf) == std::string(remote_buf)) {
            std::cerr << "[apkx] Already on latest version.\n";
            return 0;
          }
          
          std::cerr << "[apkx] Pulling updates...\n";
          system(("cd " + repo_dir + " && git reset --hard origin/main").c_str());
        }
        
        std::cerr << "[apkx] Building...\n";
        std::string build_dir = repo_dir + "/build";
        std::filesystem::create_directories(build_dir);
        system(("rm -rf " + build_dir + "/*").c_str());
        int rc = system(("cd " + build_dir + " && cmake .. && make -j4").c_str());
        if (rc != 0) {
          std::cerr << "[apkx] Build failed.\n";
          return 1;
        }
        
        system(("cp " + repo_dir + "/build/apkx " + install_dir + "/apkx && chmod +x " + install_dir + "/apkx").c_str());
        std::cerr << "[apkx] Updated to latest version!\n";
        return 0;
      }
      
      // apkx update apps [package]
      if (subcmd == "apps") {
        std::string pkg_to_update;
        if (args.positionals.size() > 1) {
          pkg_to_update = args.positionals[1];
        }
        
        auto paths = default_paths();
        auto reg = load_registry(paths.registry_json);
        
        if (reg.apps.empty()) {
          std::cerr << "[apkx] No apps installed.\n";
          return 0;
        }
        
        auto cache_dir = std::filesystem::path(std::getenv("HOME")) / ".android_compat" / "cache";
        std::filesystem::create_directories(cache_dir);
        
        int updated = 0;
        
        for (const auto& [pkg, app] : reg.apps) {
          if (!pkg_to_update.empty() && pkg != pkg_to_update) continue;
          
          std::cerr << "[apkx] Updating " << pkg << "...\n";
          
          std::string cmd = "~/.cargo/bin/apkeep -a " + pkg + " -d apk-pure " + cache_dir.string();
          int rc = system(cmd.c_str());
          
          if (rc != 0) {
            std::cerr << "[apkx] Failed to download " << pkg << "\n";
            continue;
          }
          
          std::filesystem::path apk_path = cache_dir / (pkg + ".apk");
          if (!std::filesystem::exists(apk_path)) {
            apk_path = cache_dir / (pkg + ".xapk");
          }
          
          if (!std::filesystem::exists(apk_path)) {
            std::cerr << "[apkx] Downloaded file not found for " << pkg << "\n";
            continue;
          }
          
          // Re-install (overwrite)
          try {
            auto new_app = install_apk(paths, reg, apk_path);
            reg.apps[pkg] = new_app;
            std::cerr << "[apkx] Updated " << pkg << "\n";
            updated++;
          } catch (const std::exception& e) {
            std::cerr << "[apkx] Failed to install " << pkg << ": " << e.what() << "\n";
          }
        }
        
        save_registry(paths.registry_json, reg);
        std::cerr << "[apkx] Updated " << updated << " app(s).\n";
        return 0;
      }
      
      std::cerr << "[apkx] Unknown update target: " << subcmd << "\n";
      std::cerr << "[apkx] Use: apkx update apkx | apps [package]\n";
      return 1;
    }

  usage();
  return 1;
}

} // namespace apkx