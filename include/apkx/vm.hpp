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
    MOVE_FROM16 = 0x02,
    MOVE_16 = 0x03,
    MOVE_WIDE = 0x04,
    MOVE_WIDE_FROM16 = 0x05,
    MOVE_WIDE_16 = 0x06,
    MOVE_OBJECT = 0x07,
    MOVE_OBJECT_FROM16 = 0x08,
    MOVE_OBJECT_16 = 0x09,
    MOVE_RESULT = 0x0a,
    MOVE_RESULT_WIDE = 0x0b,
    MOVE_RESULT_OBJECT = 0x0c,
    MOVE_EXCEPTION = 0x0d,
    RETURN_VOID = 0x0e,
    RETURN = 0x0f,
    RETURN_WIDE = 0x10,
    RETURN_OBJECT = 0x11,
    CONST_4 = 0x12,
    CONST_16 = 0x13,
    CONST = 0x14,
    CONST_HIGH16 = 0x15,
    CONST_WIDE_16 = 0x16,
    CONST_WIDE_32 = 0x17,
    CONST_WIDE = 0x18,
    CONST_WIDE_HIGH16 = 0x19,
    CONST_STRING = 0x1a,
    CONST_STRING_JUMBO = 0x1b,
    CONST_CLASS = 0x1c,
    CHECK_CAST = 0x1f,
    INSTANCE_OF = 0x20,
    NEW_INSTANCE = 0x22,
    NEW_ARRAY = 0x23,
    FILLED_NEW_ARRAY = 0x24,
    FILLED_NEW_ARRAY_RANGE = 0x25,
    FILL_ARRAY_DATA = 0x26,
    THROW = 0x27,
    GOTO = 0x28,
    GOTO_16 = 0x29,
    GOTO_32 = 0x2a,
    PACKED_SWITCH = 0x2b,
    SPARSE_SWITCH = 0x2c,
    CMPL_FLOAT = 0x2d,
    CMPG_FLOAT = 0x2e,
    CMPL_DOUBLE = 0x2f,
    CMPG_DOUBLE = 0x30,
    CMP_LONG = 0x31,
    IF_EQ = 0x32,
    IF_NE = 0x33,
    IF_LT = 0x34,
    IF_GE = 0x35,
    IF_GT = 0x36,
    IF_LE = 0x37,
    IF_EQZ = 0x38,
    IF_NEZ = 0x39,
    IF_LTZ = 0x3a,
    IF_GEZ = 0x3b,
    IF_GTZ = 0x3c,
    IF_LEZ = 0x3d,
    AGET = 0x44,
    AGET_WIDE = 0x45,
    AGET_OBJECT = 0x46,
    AGET_BOOLEAN = 0x47,
    AGET_BYTE = 0x48,
    AGET_CHAR = 0x49,
    AGET_SHORT = 0x4a,
    APUT = 0x4f,
    APUT_WIDE = 0x50,
    APUT_OBJECT = 0x51,
    APUT_BOOLEAN = 0x52,
    APUT_BYTE = 0x53,
    APUT_CHAR = 0x54,
    APUT_SHORT = 0x55,
    SPUT = 0x6b,
    SPUT_WIDE = 0x6c,
    SPUT_OBJECT = 0x6d,
    SPUT_BOOLEAN = 0x6e,
    SPUT_BYTE = 0x6f,
    SPUT_CHAR = 0x70,
    SPUT_SHORT = 0x71,
    INVOKE_VIRTUAL = 0x76,
    INVOKE_SUPER = 0x77,
    INVOKE_DIRECT = 0x78,
    INVOKE_STATIC = 0x79,
    INVOKE_INTERFACE = 0x7a,
    INVOKE_VIRTUAL_RANGE = 0x7b,
    INVOKE_SUPER_RANGE = 0x7c,
    INVOKE_DIRECT_RANGE = 0x7d,
    INVOKE_STATIC_RANGE = 0x7e,
    INVOKE_INTERFACE_RANGE = 0x7f,
    NEG_INT = 0x7b,
    NOT_INT = 0x7c,
    NEG_LONG = 0x7d,
    NOT_LONG = 0x7e,
    NEG_FLOAT = 0x7f,
    NEG_DOUBLE = 0x80,
    INT_TO_LONG = 0x81,
    INT_TO_FLOAT = 0x82,
    INT_TO_DOUBLE = 0x83,
    LONG_TO_INT = 0x84,
    LONG_TO_FLOAT = 0x85,
    LONG_TO_DOUBLE = 0x86,
    FLOAT_TO_INT = 0x87,
    FLOAT_TO_LONG = 0x88,
    FLOAT_TO_DOUBLE = 0x89,
    DOUBLE_TO_INT = 0x8a,
    DOUBLE_TO_LONG = 0x8b,
    DOUBLE_TO_FLOAT = 0x8c,
    INT_TO_BYTE = 0x8d,
    INT_TO_CHAR = 0x8e,
    INT_TO_SHORT = 0x8f,
    ADD_INT_2ADDR = 0x90,
    SUB_INT_2ADDR = 0x91,
    MUL_INT_2ADDR = 0x92,
    DIV_INT_2ADDR = 0x93,
    REM_INT_2ADDR = 0x94,
    AND_INT_2ADDR = 0x95,
    OR_INT_2ADDR = 0x96,
    XOR_INT_2ADDR = 0x97,
    SHL_INT_2ADDR = 0x98,
    SHR_INT_2ADDR = 0x99,
    USHR_INT_2ADDR = 0x9a,
    ADD_LONG_2ADDR = 0x9b,
    SUB_LONG_2ADDR = 0x9c,
    MUL_LONG_2ADDR = 0x9d,
    DIV_LONG_2ADDR = 0x9e,
    REM_LONG_2ADDR = 0x9f,
    AND_LONG_2ADDR = 0xa0,
    OR_LONG_2ADDR = 0xa1,
    XOR_LONG_2ADDR = 0xa2,
    SHL_LONG_2ADDR = 0xa3,
    SHR_LONG_2ADDR = 0xa4,
    USHR_LONG_2ADDR = 0xa5,
    ADD_FLOAT_2ADDR = 0xa6,
    SUB_FLOAT_2ADDR = 0xa7,
    MUL_FLOAT_2ADDR = 0xa8,
    DIV_FLOAT_2ADDR = 0xa9,
    ADD_DOUBLE_2ADDR = 0xab,
    SUB_DOUBLE_2ADDR = 0xac,
    MUL_DOUBLE_2ADDR = 0xad,
    DIV_DOUBLE_2ADDR = 0xae,
    ADD_INT_LIT16 = 0xd0,
    SUB_INT_LIT16 = 0xd1,
    MUL_INT_LIT16 = 0xd2,
    DIV_INT_LIT16 = 0xd3,
    REM_INT_LIT16 = 0xd4,
    AND_INT_LIT16 = 0xd5,
    OR_INT_LIT16 = 0xd6,
    XOR_INT_LIT16 = 0xd7,
    ADD_INT_LIT8 = 0xd8,
    SUB_INT_LIT8 = 0xd9,
    MUL_INT_LIT8 = 0xda,
    DIV_INT_LIT8 = 0xdb,
    REM_INT_LIT8 = 0xdc,
    AND_INT_LIT8 = 0xdd,
    OR_INT_LIT8 = 0xde,
    XOR_INT_LIT8 = 0xdf,
    SHL_INT_LIT8 = 0xe0,
    SHR_INT_LIT8 = 0xe1,
    USHR_INT_LIT8 = 0xe2,
    SGET = 0x60,
    SGET_WIDE = 0x61,
    SGET_OBJECT = 0x62,
    SGET_BOOLEAN = 0x63,
    SGET_BYTE = 0x64,
    SGET_CHAR = 0x65,
    SGET_SHORT = 0x66,
    IGET = 0x52,
    IGET_WIDE = 0x53,
    IGET_OBJECT = 0x54,
    IGET_BOOLEAN = 0x55,
    IGET_BYTE = 0x56,
    IGET_CHAR = 0x57,
    IGET_SHORT = 0x58,
    IPUT = 0x59,
    IPUT_WIDE = 0x5a,
    IPUT_OBJECT = 0x5b,
    IPUT_BOOLEAN = 0x5c,
    IPUT_BYTE = 0x5d,
    IPUT_CHAR = 0x5e,
    IPUT_SHORT = 0x5f,
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
