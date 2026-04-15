#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <map>
#include <stack>
#include <functional>

#include "apkx/dex_loader.hpp"
#include "apkx/vm.hpp"

namespace apkx {
namespace runtime {

// Complete Dalvik opcode set
enum class DexOp : uint8_t {
    // 0x00: NOP
    NOP = 0x00,
    
    // 0x01-0x0d: MOVE
    MOVE = 0x01, MOVE_FROM16 = 0x02, MOVE_16 = 0x03,
    MOVE_WIDE = 0x04, MOVE_WIDE_FROM16 = 0x05, MOVE_WIDE_16 = 0x06,
    MOVE_OBJECT = 0x07, MOVE_OBJECT_FROM16 = 0x08, MOVE_OBJECT_16 = 0x09,
    
    // 0x0a-0x0d: MOVE_RESULT
    MOVE_RESULT = 0x0a, MOVE_RESULT_WIDE = 0x0b,
    MOVE_RESULT_OBJECT = 0x0c, MOVE_EXCEPTION = 0x0d,
    
    // 0x0e-0x11: RETURN
    RETURN_VOID = 0x0e, RETURN = 0x0f,
    RETURN_WIDE = 0x10, RETURN_OBJECT = 0x11,
    
    // 0x12-0x1f: CONST
    CONST_4 = 0x12, CONST_16 = 0x13, CONST = 0x14,
    CONST_HIGH16 = 0x15, CONST_WIDE_16 = 0x16, CONST_WIDE_32 = 0x17,
    CONST_WIDE = 0x18, CONST_WIDE_HIGH16 = 0x19,
    CONST_STRING = 0x1a, CONST_STRING_JUMBO = 0x1b,
    CONST_CLASS = 0x1c, CONST_METHOD_HANDLE = 0x1d,
    CONST_METHOD_TYPE = 0x1e,
    
    // 0x1f-0x22: TYPE_CHECK
    CHECK_CAST = 0x1f, INSTANCE_OF = 0x20,
    
    // 0x21-0x23: ARRAY
    NEW_INSTANCE = 0x22, NEW_ARRAY = 0x23,
    
    // 0x24-0x26: FILLED_ARRAY
    FILLED_NEW_ARRAY = 0x24, FILLED_NEW_ARRAY_RANGE = 0x25,
    FILL_ARRAY_DATA = 0x26,
    
    // 0x27: THROW
    THROW = 0x27,
    
    // 0x28-0x2f: GOTO
    GOTO = 0x28, GOTO_16 = 0x29, GOTO_32 = 0x2a,
    PACKED_SWITCH = 0x2b, SPARSE_SWITCH = 0x2c,
    
    // 0x2d-0x31: COMPARE
    CMPL_FLOAT = 0x2d, CMPG_FLOAT = 0x2e,
    CMPL_DOUBLE = 0x2f, CMPG_DOUBLE = 0x30, CMP_LONG = 0x31,
    
    // 0x32-0x37: IF
    IF_EQ = 0x32, IF_NE = 0x33, IF_LT = 0x34,
    IF_GE = 0x35, IF_GT = 0x36, IF_LE = 0x37,
    
    // 0x38-0x3d: IFZ (if-zero)
    IF_EQZ = 0x38, IF_NEZ = 0x39, IF_LTZ = 0x3a,
    IF_GEZ = 0x3b, IF_GTZ = 0x3c, IF_LEZ = 0x3d,
    
    // 0x44-0x51: AGET (array get)
    AGET = 0x44, AGET_WIDE = 0x45, AGET_OBJECT = 0x46,
    AGET_BOOLEAN = 0x47, AGET_BYTE = 0x48, AGET_CHAR = 0x49,
    AGET_SHORT = 0x4a, AGET_INT = 0x4b, AGET_LONG = 0x4c,
    AGET_FLOAT = 0x4d, AGET_DOUBLE = 0x4e,
    
    // 0x4f-0x5b: APUT (array put)
    APUT = 0x4f, APUT_WIDE = 0x50, APUT_OBJECT = 0x51,
    APUT_BOOLEAN = 0x52, APUT_BYTE = 0x53, APUT_CHAR = 0x54,
    APUT_SHORT = 0x55, APUT_INT = 0x56, APUT_LONG = 0x57,
    APUT_FLOAT = 0x58, APUT_DOUBLE = 0x59,
    
    // 0x60-0x6b: SGET (static get)
    SGET = 0x60, SGET_WIDE = 0x61, SGET_OBJECT = 0x62,
    SGET_BOOLEAN = 0x63, SGET_BYTE = 0x64, SGET_CHAR = 0x65,
    SGET_SHORT = 0x66, SGET_INT = 0x67, SGET_LONG = 0x68,
    SGET_FLOAT = 0x69, SGET_DOUBLE = 0x6a,
    
    // 0x6b-0x76: SPUT (static put)
    SPUT = 0x6b, SPUT_WIDE = 0x6c, SPUT_OBJECT = 0x6d,
    SPUT_BOOLEAN = 0x6e, SPUT_BYTE = 0x6f, SPUT_CHAR = 0x70,
    SPUT_SHORT = 0x71, SPUT_INT = 0x72, SPUT_LONG = 0x73,
    SPUT_FLOAT = 0x74, SPUT_DOUBLE = 0x75,
    
    // 0x76-0x7b: INVOKE (method invocation)
    INVOKE_VIRTUAL = 0x76, INVOKE_SUPER = 0x77, INVOKE_DIRECT = 0x78,
    INVOKE_STATIC = 0x79, INVOKE_INTERFACE = 0x7a, INVOKE_VIRTUAL_RANGE = 0x7b,
    INVOKE_SUPER_RANGE = 0x7c, INVOKE_DIRECT_RANGE = 0x7d,
    INVOKE_STATIC_RANGE = 0x7e, INVOKE_INTERFACE_RANGE = 0x7f,
    
    // 0x80-0x8f: NEG/NOT
    NEG_INT = 0x7b, NOT_INT = 0x7c,
    NEG_LONG = 0x7d, NOT_LONG = 0x7e,
    NEG_FLOAT = 0x7f, NEG_DOUBLE = 0x80,
    INT_TO_LONG = 0x81, INT_TO_FLOAT = 0x82, INT_TO_DOUBLE = 0x83,
    LONG_TO_INT = 0x84, LONG_TO_FLOAT = 0x85, LONG_TO_DOUBLE = 0x86,
    FLOAT_TO_INT = 0x87, FLOAT_TO_LONG = 0x88, FLOAT_TO_DOUBLE = 0x89,
    DOUBLE_TO_INT = 0x8a, DOUBLE_TO_LONG = 0x8b, DOUBLE_TO_FLOAT = 0x8c,
    INT_TO_BYTE = 0x8d, INT_TO_CHAR = 0x8e, INT_TO_SHORT = 0x8f,
    
    // 0x90-0xaf: MATH (2addr format)
    ADD_INT_2ADDR = 0x90, SUB_INT_2ADDR = 0x91, MUL_INT_2ADDR = 0x92,
    DIV_INT_2ADDR = 0x93, REM_INT_2ADDR = 0x94, AND_INT_2ADDR = 0x95,
    OR_INT_2ADDR = 0x96, XOR_INT_2ADDR = 0x97, SHL_INT_2ADDR = 0x98,
    SHR_INT_2ADDR = 0x99, USHR_INT_2ADDR = 0x9a,
    ADD_LONG_2ADDR = 0x9b, SUB_LONG_2ADDR = 0x9c, MUL_LONG_2ADDR = 0x9d,
    DIV_LONG_2ADDR = 0x9e, REM_LONG_2ADDR = 0x9f, AND_LONG_2ADDR = 0xa0,
    OR_LONG_2ADDR = 0xa1, XOR_LONG_2ADDR = 0xa2, SHL_LONG_2ADDR = 0xa3,
    SHR_LONG_2ADDR = 0xa4, USHR_LONG_2ADDR = 0xa5,
    ADD_FLOAT_2ADDR = 0xa6, SUB_FLOAT_2ADDR = 0xa7, MUL_FLOAT_2ADDR = 0xa8,
    DIV_FLOAT_2ADDR = 0xa9, REM_FLOAT_2ADDR = 0xaa,
    ADD_DOUBLE_2ADDR = 0xab, SUB_DOUBLE_2ADDR = 0xac, MUL_DOUBLE_2ADDR = 0xad,
    DIV_DOUBLE_2ADDR = 0xae, REM_DOUBLE_2ADDR = 0xaf,
    
    // 0xb0-0xcf: MATH (lit16 format)
    ADD_INT_LIT16 = 0xd0, SUB_INT_LIT16 = 0xd1, MUL_INT_LIT16 = 0xd2,
    DIV_INT_LIT16 = 0xd3, REM_INT_LIT16 = 0xd4, AND_INT_LIT16 = 0xd5,
    OR_INT_LIT16 = 0xd6, XOR_INT_LIT16 = 0xd7,
    
    // 0xd8-0xe2: MATH (lit8 format)
    ADD_INT_LIT8 = 0xd8, SUB_INT_LIT8 = 0xd9, MUL_INT_LIT8 = 0xda,
    DIV_INT_LIT8 = 0xdb, REM_INT_LIT8 = 0xdc, AND_INT_LIT8 = 0xdd,
    OR_INT_LIT8 = 0xde, XOR_INT_LIT8 = 0xdf, SHL_INT_LIT8 = 0xe0,
    SHR_INT_LIT8 = 0xe1, USHR_INT_LIT8 = 0xe2,
    
    // Extended opcodes (needed for newer Android versions)
    INVOKE_POLYMORPHIC = 0xfa, INVOKE_POLYMORPHIC_RANGE = 0xfb,
    INVOKE_CUSTOM = 0xfc, INVOKE_CUSTOM_RANGE = 0xfd,
    CONST_METHOD_HANDLE_2 = 0xfe, CONST_METHOD_TYPE_2 = 0xff
};

// Method execution context
struct ExecutionContext {
    std::vector<RegisterValue> registers;
    uint32_t pc = 0;
    bool running = false;
    RegisterValue result;
    std::string error;
    
    ExecutionContext(size_t reg_count) : registers(reg_count) {}
};

// The DEX interpreter - executes Dalvik bytecode
class DexInterpreter {
public:
    DexInterpreter(DexLoader& dex);
    ~DexInterpreter();
    
    // Execute a method by index
    ExecutionResult executeMethod(uint32_t method_idx, 
                                   const std::vector<RegisterValue>& args);
    
    // Execute a method by name
    ExecutionResult executeMethod(const std::string& class_name,
                                   const std::string& method_name,
                                   const std::vector<RegisterValue>& args);
    
    // Step through one instruction (for debugging)
    bool step(ExecutionContext& ctx);
    
    // Find main() method
    bool findMainMethod(uint32_t& method_idx);
    
    // Debug
    void setDebugMode(bool debug) { debug_mode_ = debug; }
    void dumpState(const ExecutionContext& ctx);

private:
    DexLoader& dex_;
    bool debug_mode_ = false;
    
    // Opcode dispatch table
    using OpHandler = std::function<bool(ExecutionContext&, const uint8_t*, uint32_t&)>;
    std::map<DexOp, OpHandler> handlers_;
    
    void registerHandlers();
    
    // Individual opcode implementations
    bool handleNop(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleMove(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleConst(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleReturn(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleInvoke(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleIf(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleGoto(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleMath(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleArray(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleField(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleNew(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    bool handleConvert(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc);
    
    // Helper methods
    const uint8_t* getMethodCode(uint32_t method_idx, size_t& code_size);
    uint32_t readULEB128(const uint8_t*& p);
    int32_t readSLEB128(const uint8_t*& p);
    int32_t signExtend(uint32_t value, int bits);
    
    // Method resolution
    bool resolveMethod(uint32_t method_idx, std::string& class_name, 
                      std::string& method_name, std::string& signature);
    bool isNativeMethod(uint32_t method_idx);
    ExecutionResult callNative(uint32_t method_idx, 
                               const std::vector<RegisterValue>& args);
};

} // namespace runtime
} // namespace apkx
