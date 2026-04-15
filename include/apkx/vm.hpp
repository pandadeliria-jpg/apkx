#pragma once

#include <cstdint>
#include <vector>
#include <stack>
#include <memory>
#include <map>
#include <string>

namespace apkx {
namespace runtime {

// Dalvik/ART bytecode opcodes (subset)
enum class OpCode : uint8_t {
    NOP = 0x00,
    MOVE = 0x01,
    MOVE_RESULT = 0x0a,
    RETURN_VOID = 0x0e,
    RETURN = 0x0f,
    CONST_4 = 0x12,
    CONST_16 = 0x13,
    CONST = 0x14,
    CONST_HIGH16 = 0x15,
    CONST_STRING = 0x1a,
    CONST_CLASS = 0x1c,
    SGET = 0x60,
    SPUT = 0x67,
    INVOKE_VIRTUAL = 0x6e,
    INVOKE_SUPER = 0x6f,
    INVOKE_DIRECT = 0x70,
    INVOKE_STATIC = 0x71,
    INVOKE_INTERFACE = 0x72,
    NEW_INSTANCE = 0x22,
    NEW_ARRAY = 0x23,
    CHECK_CAST = 0x1f,
    INSTANCE_OF = 0x20,
    GOTO = 0x28,
    IF_EQ = 0x32,
    IF_NE = 0x33,
    AGET = 0x44,
    APUT = 0x4b,
    ADD_INT = 0x90,
    MUL_INT = 0x92,
    // ... many more opcodes
};

// Register value types (Dalvik is register-based, not stack-based)
union RegisterValue {
    int32_t i;
    int64_t j;
    float f;
    double d;
    void* obj;  // Object reference
    
    RegisterValue() : i(0) {}
};

// Stack frame for method execution
struct Frame {
    uint32_t method_idx;
    std::vector<RegisterValue> registers;
    std::vector<uint32_t> ins;   // Input arguments
    std::vector<uint32_t> outs;  // Output arguments for calls
    uint32_t pc = 0;             // Program counter
    Frame* parent = nullptr;
    
    Frame(size_t reg_count) : registers(reg_count) {}
};

// Object header (simplified)
struct Object {
    uint32_t class_idx;
    uint32_t monitor;
    std::map<std::string, RegisterValue> fields;
    
    Object(uint32_t cls) : class_idx(cls), monitor(0) {}
};

// Method execution result
struct ExecutionResult {
    bool success;
    RegisterValue value;
    std::string error;
    
    ExecutionResult() : success(false) {}
    ExecutionResult(RegisterValue v) : success(true), value(v) {}
};

// The Virtual Machine - executes DEX bytecode
class VM {
public:
    VM();
    ~VM();
    
    // Frame management
    void pushFrame(std::unique_ptr<Frame> frame);
    void popFrame();
    Frame* currentFrame();
    
    // Object heap (simplified)
    Object* allocateObject(uint32_t class_idx);
    Object* getObject(void* ref);
    void gc();  // Simple mark-and-sweep
    
    // Execution
    ExecutionResult execute(const uint8_t* code, size_t code_size);
    ExecutionResult executeMethod(uint32_t method_idx);
    
    // Exception handling
    void throwException(const std::string& class_name, const std::string& msg);
    bool hasPendingException() const;
    void clearException();
    
    // Debug
    void dumpState();

private:
    std::stack<std::unique_ptr<Frame>> call_stack_;
    std::map<void*, std::unique_ptr<Object>> heap_;
    void* next_obj_id_ = reinterpret_cast<void*>(0x1000);
    
    bool exception_pending_ = false;
    std::string exception_class_;
    std::string exception_msg_;
    
    // Opcode handlers
    ExecutionResult executeInstruction(const uint8_t* code, uint32_t& pc);
    
    // Individual opcode implementations
    void opMove(uint8_t dst, uint8_t src);
    void opConst4(uint8_t dst, int8_t value);
    void opConst16(uint8_t dst, int16_t value);
    void opConstString(uint8_t dst, uint32_t string_idx);
    void opNewInstance(uint8_t dst, uint32_t class_idx);
    ExecutionResult opInvoke(uint8_t opcode, uint8_t argc, uint16_t method_idx);
    void opReturn(uint8_t opcode, uint8_t reg);
    void opGoto(int16_t offset);
    void opIf(uint8_t opcode, uint8_t a, uint8_t b, int16_t offset);
};

} // namespace runtime
} // namespace apkx
