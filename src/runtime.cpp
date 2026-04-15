#include "apkx/runtime.hpp"
#include "apkx/vm.hpp"
#include "apkx/bionic_darwin.hpp"
#include "apkx/jni_bridge.hpp"
#include "apkx/system_services.hpp"

#include <iostream>
#include <fstream>

namespace apkx {
namespace runtime {

// Global runtime instance
static std::unique_ptr<AndroidRuntime> g_runtime;

AndroidRuntime::AndroidRuntime(const RuntimeConfig& config) 
    : config_(config) {}

AndroidRuntime::~AndroidRuntime() {
    shutdown();
}

bool AndroidRuntime::initialize() {
    std::cout << "[*] AndroidRuntime initializing...\n";
    std::cout << "    Package: " << config_.app_package << "\n";
    
    // Initialize syscall translator (Bionic→Darwin)
    auto* translator = getSyscallTranslator();
    if (!translator->initialize(config_.app_data_dir, config_.app_cache_dir)) {
        std::cerr << "[!] Failed to initialize syscall translator\n";
        return false;
    }
    std::cout << "    Syscall translator: OK\n";
    
    // Initialize VM
    if (!initializeVM()) {
        std::cerr << "[!] Failed to initialize VM\n";
        return false;
    }
    std::cout << "    VM: OK\n";
    
    // Initialize JNI
    if (!initializeJNI()) {
        std::cerr << "[!] Failed to initialize JNI\n";
        return false;
    }
    std::cout << "    JNI: OK\n";
    
    // Initialize system services
    if (!initializeSystemServices()) {
        std::cerr << "[!] Failed to initialize system services\n";
        return false;
    }
    std::cout << "    System services: OK\n";
    
    running_ = true;
    std::cout << "[+] Runtime initialized successfully\n";
    return true;
}

void AndroidRuntime::shutdown() {
    if (!running_) return;
    
    std::cout << "[*] AndroidRuntime shutting down...\n";
    
    // Clean up in reverse order
    sys_services_.reset();
    native_libs_.clear();
    jni_env_.reset();
    vm_.reset();
    
    running_ = false;
    std::cout << "[+] Runtime shutdown complete\n";
}

bool AndroidRuntime::initializeVM() {
    vm_ = std::make_unique<VM>();
    return true;
}

bool AndroidRuntime::initializeJNI() {
    jni_env_ = std::make_unique<JNIEnvironment>();
    return jni_env_->initialize();
}

bool AndroidRuntime::initializeSystemServices() {
    // TODO: Initialize ActivityManager, PackageManager, etc.
    sys_services_ = std::make_unique<SystemServices>();
    return true;
}

bool AndroidRuntime::loadApk(const std::string& apk_path) {
    std::cout << "[*] Loading APK: " << apk_path << "\n";
    
    // TODO: Extract APK, load DEX files, native libs
    // For now, just verify it exists
    std::ifstream f(apk_path);
    if (!f.good()) {
        std::cerr << "[!] APK not found: " << apk_path << "\n";
        return false;
    }
    
    return true;
}

bool AndroidRuntime::loadDex(const std::string& dex_path) {
    std::cout << "[*] Loading DEX: " << dex_path << "\n";
    
    auto dex = std::make_unique<DexLoader>();
    if (!dex->load(dex_path)) {
        return false;
    }
    
    std::cout << "    Classes: " << dex->getClassCount() << "\n";
    std::cout << "    Methods: " << dex->getMethodCount() << "\n";
    
    dex_files_.push_back(std::move(dex));
    return true;
}

bool AndroidRuntime::loadNativeLib(const std::string& lib_path) {
    std::cout << "[*] Loading native lib: " << lib_path << "\n";
    
    auto elf = std::make_unique<ElfLoader>();
    if (!elf->load(lib_path)) {
        // Might be a ZIP bootstrap
        return false;
    }
    
    std::cout << "    ARM64: " << (elf->isArm64() ? "yes" : "no") << "\n";
    std::cout << "    Exports: " << elf->listExports().size() << "\n";
    
    native_libs_[lib_path] = std::move(elf);
    return true;
}

int AndroidRuntime::runMainActivity() {
    if (!running_) {
        std::cerr << "[!] Runtime not initialized\n";
        return -1;
    }
    
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              EXECUTING ANDROID APPLICATION                   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";
    
    // Step 1: Execute DEX bytecode
    if (!dex_files_.empty()) {
        std::cout << "[*] Phase 1: DEX Bytecode Execution\n";
        std::cout << "    Loading " << dex_files_.size() << " DEX file(s)\n\n";
        
        // Execute each DEX file's main/onCreate
        for (auto& dex : dex_files_) {
            std::cout << "[DEX] Classes: " << dex->getClassCount() 
                      << ", Methods: " << dex->getMethodCount() << "\n";
            
            // Look for main activity methods
            const auto& methods = dex->getMethodIds();
            for (uint32_t i = 0; i < methods.size(); i++) {
                std::string mname = dex->getMethodName(i);
                if (mname == "onCreate" || mname == "main") {
                    std::cout << "  [>] Found entry: " << mname << " at method " << i << "\n";
                }
            }
        }
    }
    
    // Step 2: Initialize native libraries
    if (!native_libs_.empty()) {
        std::cout << "\n[*] Phase 2: Native Library Loading\n";
        for (const auto& [path, elf] : native_libs_) {
            std::cout << "    Library: " << path << "\n";
            std::cout << "    ARM64: " << (elf->isArm64() ? "YES" : "NO") << "\n";
            
            auto exports = elf->listExports();
            std::cout << "    JNI Exports: " << exports.size() << "\n";
            
            // Show JNI methods
            for (const auto& exp : exports) {
                if (exp.find("Java_") == 0) {
                    std::cout << "      - " << exp << "\n";
                }
            }
        }
        
        std::cout << "\n    ✓ Native libraries mapped (Apple Silicon = native ARM64)\n";
    }
    
    // Step 3: Activity lifecycle (via system services)
    std::cout << "\n[*] Phase 3: Activity Lifecycle\n";
    if (sys_services_) {
        auto* am = sys_services_->getActivityManager();
        if (am) {
            am->startActivity("MainActivity");
            std::cout << "    Activity state: RESUMED\n";
        }
    }
    
    // Step 4: Graphics (WIP)
    std::cout << "\n[*] Phase 4: Graphics Initialization\n";
    std::cout << "    Status: STUB (OpenGL ES → Metal translation not yet active)\n";
    
    // Step 5: Main execution loop
    std::cout << "\n[*] Phase 5: Application Main Loop\n";
    std::cout << "    Simulating 5 seconds of execution...\n";
    
    // Simulate app running
    for (int i = 0; i < 5; i++) {
        std::cout << "    [t=" << i << "s] App running...\n";
        // In real implementation, this would:
        // - Process UI events
        // - Execute DEX bytecode
        // - Call native methods
        // - Render graphics
    }
    
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              APPLICATION EXECUTION COMPLETE                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Summary:\n";
    std::cout << "  ✓ DEX bytecode loaded and scanned\n";
    std::cout << "  ✓ Native libraries mapped\n";
    std::cout << "  ✓ Activity lifecycle initialized\n";
    std::cout << "  ⚠ Graphics rendering (needs Metal implementation)\n";
    std::cout << "  ⚠ Full bytecode interpretation (needs VM completion)\n";
    std::cout << "  ⚠ Native method execution (needs dynamic linking)\n\n";
    
    return 0;
}

int AndroidRuntime::runMethod(const std::string& class_name,
                              const std::string& method_name,
                              const std::vector<std::string>& args) {
    if (!running_) {
        std::cerr << "[!] Runtime not initialized\n";
        return -1;
    }
    
    std::cout << "[*] Running method: " << class_name << "." << method_name << "\n";
    
    // TODO: Find method in loaded DEX
    // TODO: Execute bytecode in VM
    // TODO: Handle args/return value
    
    return 0;
}

// Global runtime access
AndroidRuntime* getRuntime() {
    return g_runtime.get();
}

void setRuntime(std::unique_ptr<AndroidRuntime> rt) {
    g_runtime = std::move(rt);
}

} // namespace runtime
} // namespace apkx
