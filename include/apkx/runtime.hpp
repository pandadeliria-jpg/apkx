#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <map>

#include "apkx/dex_loader.hpp"
#include "apkx/elf_loader.hpp"

namespace apkx {
namespace runtime {

// Forward declarations
class VM;
class JNIEnvironment;
class SystemServices;

// Runtime configuration
struct RuntimeConfig {
    std::string app_package;
    std::string app_data_dir;
    std::string app_cache_dir;
    std::vector<std::string> native_lib_paths;
    bool debug_mode = false;
    bool enable_jit = false;  // Future: LLVM-based JIT
};

// Android Runtime - The core compatibility layer
class AndroidRuntime {
public:
    AndroidRuntime(const RuntimeConfig& config);
    ~AndroidRuntime();

    // Lifecycle
    bool initialize();
    void shutdown();
    bool isRunning() const { return running_; }

    // App loading
    bool loadApk(const std::string& apk_path);
    bool loadDex(const std::string& dex_path);
    bool loadNativeLib(const std::string& lib_path);

    // Execution
    int runMainActivity();
    int runMethod(const std::string& class_name, 
                  const std::string& method_name,
                  const std::vector<std::string>& args);

    // JNI bridge
    JNIEnvironment* getJNI() { return jni_env_.get(); }
    
    // System services
    SystemServices* getSystemServices() { return sys_services_.get(); }

    // VM access
    VM* getVM() { return vm_.get(); }

private:
    RuntimeConfig config_;
    bool running_ = false;
    
    // Core components
    std::unique_ptr<VM> vm_;
    std::unique_ptr<JNIEnvironment> jni_env_;
    std::unique_ptr<SystemServices> sys_services_;
    
    // Loaded app state
    std::vector<std::unique_ptr<DexLoader>> dex_files_;
    std::map<std::string, std::unique_ptr<ElfLoader>> native_libs_;
    
    bool initializeVM();
    bool initializeJNI();
    bool initializeSystemServices();
    bool linkNativeMethods();
};

// Global runtime instance (singleton pattern)
AndroidRuntime* getRuntime();
void setRuntime(std::unique_ptr<AndroidRuntime> rt);

} // namespace runtime
} // namespace apkx
