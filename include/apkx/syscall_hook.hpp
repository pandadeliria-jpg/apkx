#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>

namespace apkx {
namespace runtime {

// Syscall hooking mechanism
// On macOS we use ptrace to intercept syscalls from child processes
// This is critical for running ARM64 binaries

// Hooked syscall handler
using SyscallHandler = std::function<bool(uint64_t syscall_num,
                                           uint64_t arg0, uint64_t arg1,
                                           uint64_t arg2, uint64_t arg3,
                                           uint64_t arg4, uint64_t arg5,
                                           uint64_t& result)>;

// ARM64 register state
struct ARM64Registers {
    uint64_t x[32];  // General purpose registers
    uint64_t pc;     // Program counter
    uint64_t sp;     // Stack pointer
    uint64_t pstate; // Processor state
    
    uint64_t& operator[](int idx) { return x[idx]; }
    uint64_t getSyscallNum() const { return x[8]; }  // x8 = syscall number
    uint64_t getArg(int n) const { return x[n]; }  // x0-x5 = args
};

// Syscall interposer - hooks ARM64 syscalls
class SyscallInterposer {
public:
    SyscallInterposer();
    ~SyscallInterposer();
    
    // Initialize hooking mechanism
    bool initialize();
    bool isActive() const { return active_; }
    
    // Register syscall handler
    void registerHandler(uint64_t syscall_num, SyscallHandler handler);
    void registerDefaultHandler(SyscallHandler handler);
    
    // Spawn and trace a process
    bool spawnAndTrace(const std::string& executable,
                      const std::vector<std::string>& args);
    
    // Attach to existing process
    bool attachToProcess(pid_t pid);
    
    // Main event loop - processes syscalls
    void runEventLoop();
    void stopEventLoop();
    
    // Single step execution
    bool stepInstruction(pid_t pid);
    bool continueExecution(pid_t pid);
    
    // Register manipulation
    bool getRegisters(pid_t pid, ARM64Registers& regs);
    bool setRegisters(pid_t pid, const ARM64Registers& regs);
    
    // Memory access
    bool readMemory(pid_t pid, uint64_t addr, void* buf, size_t size);
    bool writeMemory(pid_t pid, uint64_t addr, const void* buf, size_t size);

private:
    bool active_ = false;
    bool running_ = false;
    
    std::map<uint64_t, SyscallHandler> handlers_;
    SyscallHandler default_handler_;
    
    pid_t traced_pid_ = -1;
    
    // Platform-specific implementation
    bool setupPtrace();
    bool waitForSyscall(pid_t pid);
    bool handleSyscallEntry(pid_t pid);
    bool handleSyscallExit(pid_t pid);
    
    // macOS-specific
    #ifdef __APPLE__
    bool initMachPort();
    void* mach_port_ = nullptr;
    #endif
};

// Native code loader - loads and patches ARM64 binaries
class NativeLoader {
public:
    NativeLoader();
    ~NativeLoader();
    
    // Load ARM64 ELF and prepare for execution
    bool loadBinary(const std::string& path);
    
    // Patch syscalls in binary to call our handlers
    bool patchSyscalls();
    
    // Relocate binary to macOS-compatible addresses
    bool relocate();
    
    // Execute loaded binary
    int execute(int argc, char** argv);
    
    // Get entry point
    void* getEntryPoint() const { return entry_point_; }
    
private:
    std::vector<uint8_t> binary_data_;
    void* loaded_base_ = nullptr;
    void* entry_point_ = nullptr;
    
    // Find and patch svc #0x80 instructions (ARM64 syscall)
    bool findAndPatchSyscalls();
    
    // ARM64 instruction encoding
    uint32_t encodeSvc(uint16_t imm);
    uint32_t encodeBranch(uint64_t from, uint64_t to);
    uint32_t encodeNop();
};

// Combined runtime executor
class NativeExecutor {
public:
    NativeExecutor();
    ~NativeExecutor();
    
    // Initialize everything
    bool initialize();
    
    // Load and execute native library
    int executeLibrary(const std::string& lib_path,
                      const std::string& entry_symbol,
                      void* arg);
    
    // Load and execute binary
    int executeBinary(const std::string& binary_path,
                     const std::vector<std::string>& args);
    
    // JNI execution
    void* executeJNIMethod(const std::string& class_name,
                          const std::string& method_name,
                          const std::string& signature,
                          void* obj, void** args);

private:
    std::unique_ptr<SyscallInterposer> interposer_;
    std::unique_ptr<NativeLoader> loader_;
    
    bool initialized_ = false;
};

} // namespace runtime
} // namespace apkx
