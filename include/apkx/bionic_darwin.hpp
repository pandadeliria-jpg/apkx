#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <map>
#include <memory>

namespace apkx {
namespace runtime {

// Bionic (Android libc) → Darwin (macOS libc) translation
// This is the heart of the compatibility layer

// Syscall numbers (ARM64)
namespace Syscall {
    constexpr uint64_t EXIT = 93;
    constexpr uint64_t READ = 63;
    constexpr uint64_t WRITE = 64;
    constexpr uint64_t OPENAT = 56;
    constexpr uint64_t CLOSE = 57;
    constexpr uint64_t LSEEK = 62;
    constexpr uint64_t MMAP = 222;
    constexpr uint64_t MUNMAP = 215;
    constexpr uint64_t BRK = 214;
    constexpr uint64_t IOCTL = 29;
    constexpr uint64_t GETPID = 172;
    constexpr uint64_t GETTID = 178;
    constexpr uint64_t CLONE = 220;  // pthread_create / fork
    constexpr uint64_t EXECVE = 221;
    constexpr uint64_t WAIT4 = 260;
    constexpr uint64_t PIPE2 = 59;
    constexpr uint64_t DUP3 = 24;
    constexpr uint64_t RT_SIGACTION = 134;
    constexpr uint64_t FUTEX = 98;
    constexpr uint64_t GETTIME = 113;
    constexpr uint64_t SOCKET = 198;
    constexpr uint64_t CONNECT = 203;
    constexpr uint64_t SENDTO = 206;
    constexpr uint64_t RECVFROM = 207;
}

// Syscall result
struct SyscallResult {
    int64_t retval;      // Return value (or -errno on error)
    bool success;
    int error_code;        // errno equivalent
    
    SyscallResult() : retval(-1), success(false), error_code(0) {}
    SyscallResult(int64_t r) : retval(r), success(r >= 0), error_code(0) {}
};

// File descriptor mapping (Android fd → macOS fd)
class FDTable {
public:
    int map(int android_fd);
    int get(int android_fd) const;
    void set(int android_fd, int host_fd);
    void remove(int android_fd);
    int allocate();
    
private:
    std::map<int, int> fd_map_;
    int next_fd_ = 3;  // Start after stdin/stdout/stderr
};

// Path redirection for /data/data/... sandboxing
class PathRedirection {
public:
    void setAppDataPath(const std::string& android_path, 
                        const std::string& host_path);
    std::string redirect(const std::string& android_path) const;
    
private:
    std::map<std::string, std::string> redirects_;
};

// JavaVM forward declaration
struct JavaVM;

// The syscall translator
class BionicDarwinTranslator {
public:
    BionicDarwinTranslator();
    ~BionicDarwinTranslator();
    
    bool initialize(const std::string& app_data_dir,
                    const std::string& app_cache_dir);
    
    // Main syscall entry point - translates Android syscall to macOS
    SyscallResult translate(uint64_t syscall_number,
                           uint64_t arg0, uint64_t arg1, 
                           uint64_t arg2, uint64_t arg3,
                           uint64_t arg4, uint64_t arg5);
    
    // File operations with path redirection
    SyscallResult sys_openat(int dirfd, const std::string& pathname, 
                             int flags, int mode);
    SyscallResult sys_read(int fd, void* buf, size_t count);
    SyscallResult sys_write(int fd, const void* buf, size_t count);
    SyscallResult sys_close(int fd);
    SyscallResult sys_lseek(int fd, off_t offset, int whence);
    SyscallResult sys_pipe2(int* pipefd, int flags);
    SyscallResult sys_dup3(int oldfd, int newfd, int flags);
    
    // Memory operations
    SyscallResult sys_mmap(void* addr, size_t length, int prot, 
                           int flags, int fd, off_t offset);
    SyscallResult sys_munmap(void* addr, size_t length);
    SyscallResult sys_brk(void* addr);
    
    // Threading
    SyscallResult sys_clone(uint64_t flags, void* stack, 
                            void* parent_tid, void* child_tid,
                            void* tls);
    SyscallResult sys_futex(uint32_t* uaddr, int futex_op, 
                            int val, uint64_t timeout,
                            uint32_t* uaddr2, int val3);
    SyscallResult sys_rt_sigaction(int signum, const void* act, 
                                   void* oact, size_t sigsetsize);
    
    // Process
    SyscallResult sys_getpid();
    SyscallResult sys_gettid();
    SyscallResult sys_exit(int code);
    SyscallResult sys_execve(const char* filename, char* const argv[], 
                             char* const envp[]);
    SyscallResult sys_wait4(pid_t pid, int* status, int options, 
                           void* rusage);
    
    // Time
    SyscallResult sys_gettime(int clock_id, struct timespec* tp);
    
    // ioctl translation (critical for graphics/terminals)
    SyscallResult sys_ioctl(int fd, uint64_t request, void* arg);
    
    // Socket translation
    SyscallResult sys_socket(int domain, int type, int protocol);
    SyscallResult sys_connect(int sockfd, const void* addr, size_t addrlen);
    
    // Property system (Android-specific)
    int property_get(const std::string& key, char* value, size_t len);
    int property_set(const std::string& key, const std::string& value);

private:
    FDTable fd_table_;
    PathRedirection path_redirect_;
    std::map<std::string, std::string> properties_;
    
    bool initialized_ = false;
    
    // ioctl code translation tables
    std::map<uint64_t, uint64_t> ioctl_map_;
    void initIoctlMap();
};

// Global translator instance
BionicDarwinTranslator* getSyscallTranslator();

} // namespace runtime
} // namespace apkx
