#include "apkx/dex_interpreter.hpp"

#include <iostream>
#include <cstring>
#include <cmath>

namespace apkx {
namespace runtime {

DexInterpreter::DexInterpreter(DexLoader& dex) : dex_(dex) {
    registerHandlers();
}

DexInterpreter::~DexInterpreter() = default;

void DexInterpreter::registerHandlers() {
    // Register all opcode handlers
    handlers_[DexOp::NOP] = [this](auto& ctx, auto code, auto& pc) { return handleNop(ctx, code, pc); };
    handlers_[DexOp::MOVE] = [this](auto& ctx, auto code, auto& pc) { return handleMove(ctx, code, pc); };
    handlers_[DexOp::MOVE_FROM16] = [this](auto& ctx, auto code, auto& pc) { return handleMove(ctx, code, pc); };
    handlers_[DexOp::MOVE_16] = [this](auto& ctx, auto code, auto& pc) { return handleMove(ctx, code, pc); };
    handlers_[DexOp::CONST_4] = [this](auto& ctx, auto code, auto& pc) { return handleConst(ctx, code, pc); };
    handlers_[DexOp::CONST_16] = [this](auto& ctx, auto code, auto& pc) { return handleConst(ctx, code, pc); };
    handlers_[DexOp::CONST] = [this](auto& ctx, auto code, auto& pc) { return handleConst(ctx, code, pc); };
    handlers_[DexOp::RETURN_VOID] = [this](auto& ctx, auto code, auto& pc) { return handleReturn(ctx, code, pc); };
    handlers_[DexOp::RETURN] = [this](auto& ctx, auto code, auto& pc) { return handleReturn(ctx, code, pc); };
    handlers_[DexOp::INVOKE_STATIC] = [this](auto& ctx, auto code, auto& pc) { return handleInvoke(ctx, code, pc); };
    handlers_[DexOp::INVOKE_DIRECT] = [this](auto& ctx, auto code, auto& pc) { return handleInvoke(ctx, code, pc); };
    handlers_[DexOp::INVOKE_VIRTUAL] = [this](auto& ctx, auto code, auto& pc) { return handleInvoke(ctx, code, pc); };
    handlers_[DexOp::GOTO] = [this](auto& ctx, auto code, auto& pc) { return handleGoto(ctx, code, pc); };
    handlers_[DexOp::IF_EQ] = [this](auto& ctx, auto code, auto& pc) { return handleIf(ctx, code, pc); };
    handlers_[DexOp::IF_NE] = [this](auto& ctx, auto code, auto& pc) { return handleIf(ctx, code, pc); };
    handlers_[DexOp::ADD_INT_2ADDR] = [this](auto& ctx, auto code, auto& pc) { return handleMath(ctx, code, pc); };
    handlers_[DexOp::SUB_INT_2ADDR] = [this](auto& ctx, auto code, auto& pc) { return handleMath(ctx, code, pc); };
    handlers_[DexOp::MUL_INT_2ADDR] = [this](auto& ctx, auto code, auto& pc) { return handleMath(ctx, code, pc); };
    handlers_[DexOp::DIV_INT_2ADDR] = [this](auto& ctx, auto code, auto& pc) { return handleMath(ctx, code, pc); };
    handlers_[DexOp::NEW_INSTANCE] = [this](auto& ctx, auto code, auto& pc) { return handleNew(ctx, code, pc); };
    handlers_[DexOp::NEW_ARRAY] = [this](auto& ctx, auto code, auto& pc) { return handleArray(ctx, code, pc); };
    handlers_[DexOp::AGET] = [this](auto& ctx, auto code, auto& pc) { return handleArray(ctx, code, pc); };
    handlers_[DexOp::APUT] = [this](auto& ctx, auto code, auto& pc) { return handleArray(ctx, code, pc); };
    handlers_[DexOp::SGET] = [this](auto& ctx, auto code, auto& pc) { return handleField(ctx, code, pc); };
    handlers_[DexOp::SPUT] = [this](auto& ctx, auto code, auto& pc) { return handleField(ctx, code, pc); };
}

ExecutionResult DexInterpreter::executeMethod(
    uint32_t method_idx, 
    const std::vector<RegisterValue>& args) {
    
    // Get method info
    const auto& methods = dex_.getMethodIds();
    if (method_idx >= methods.size()) {
        return ExecutionResult();
    }
    
    const auto& method = methods[method_idx];
    std::string class_name = dex_.getTypeName(method.class_idx);
    std::string method_name = dex_.getMethodName(method_idx);
    
    if (debug_mode_) {
        std::cout << "[DEX] Executing method: " << class_name << "." << method_name << "\n";
    }
    
    // Get method code
    size_t code_size;
    const uint8_t* code = getMethodCode(method_idx, code_size);
    if (!code || code_size == 0) {
        if (isNativeMethod(method_idx)) {
            return callNative(method_idx, args);
        }
        std::cerr << "[!] No code for method: " << method_name << "\n";
        return ExecutionResult();
    }
    
    // Setup execution context
    uint16_t registers_size = *reinterpret_cast<const uint16_t*>(code);
    code += 2;
    uint16_t ins_size = *reinterpret_cast<const uint16_t*>(code);
    code += 2;
    uint16_t outs_size = *reinterpret_cast<const uint16_t*>(code);
    code += 2;
    uint16_t tries_size = *reinterpret_cast<const uint16_t*>(code);
    code += 4;  // Skip debug info offset
    uint32_t insns_size = *reinterpret_cast<const uint32_t*>(code);
    code += 4;
    
    ExecutionContext ctx(registers_size);
    ctx.running = true;
    
    // Copy arguments to registers (last registers)
    for (size_t i = 0; i < args.size() && i < ins_size; i++) {
        ctx.registers[registers_size - ins_size + i] = args[i];
    }
    
    // Execute instructions
    const uint8_t* insns = code;
    while (ctx.running && ctx.pc < insns_size * 2) {
        if (debug_mode_) {
            std::cout << "[DEX] PC=" << ctx.pc << " OP=" << std::hex 
                      << static_cast<int>(insns[ctx.pc]) << std::dec << "\n";
        }
        
        if (!step(ctx)) {
            break;
        }
    }
    
    return ctx.error.empty() ? ExecutionResult(ctx.result) : ExecutionResult();
}

ExecutionResult DexInterpreter::executeMethod(
    const std::string& class_name,
    const std::string& method_name,
    const std::vector<RegisterValue>& args) {
    
    // Find method by name
    const auto& methods = dex_.getMethodIds();
    for (uint32_t i = 0; i < methods.size(); i++) {
        std::string mname = dex_.getMethodName(i);
        if (mname == method_name) {
            std::string cname = dex_.getTypeName(methods[i].class_idx);
            if (cname == class_name || cname.find(class_name) != std::string::npos) {
                return executeMethod(i, args);
            }
        }
    }
    
    std::cerr << "[!] Method not found: " << class_name << "." << method_name << "\n";
    return ExecutionResult();
}

bool DexInterpreter::step(ExecutionContext& ctx) {
    // Get current instruction
    const auto& class_defs = dex_.getClassDefs();
    if (class_defs.empty()) return false;
    
    // Find method code - this is simplified, should get from class data
    uint32_t method_idx = 0;  // Should be passed or stored in context
    size_t code_size;
    const uint8_t* code = getMethodCode(method_idx, code_size);
    if (!code) return false;
    
    // Skip header to get to instructions
    code += 16;  // Skip code item header
    
    uint8_t op = code[ctx.pc];
    DexOp opcode = static_cast<DexOp>(op);
    
    auto it = handlers_.find(opcode);
    if (it != handlers_.end()) {
        return it->second(ctx, code, ctx.pc);
    } else {
        std::cerr << "[!] Unhandled opcode: 0x" << std::hex << static_cast<int>(op) << std::dec << "\n";
        ctx.pc += 2;  // Skip unknown
        return true;
    }
}

bool DexInterpreter::findMainMethod(uint32_t& method_idx) {
    const auto& methods = dex_.getMethodIds();
    for (uint32_t i = 0; i < methods.size(); i++) {
        std::string name = dex_.getMethodName(i);
        if (name == "main" || name == "onCreate") {
            method_idx = i;
            return true;
        }
    }
    return false;
}

// Opcode handlers
bool DexInterpreter::handleNop(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    pc += 2;
    return true;
}

bool DexInterpreter::handleMove(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    uint8_t op = code[pc];
    uint8_t b = code[pc + 1];
    uint8_t dst, src;
    
    if (op == static_cast<uint8_t>(DexOp::MOVE)) {
        dst = b & 0x0f;
        src = (b >> 4) & 0x0f;
        pc += 2;
    } else if (op == static_cast<uint8_t>(DexOp::MOVE_FROM16)) {
        dst = b;
        src = *reinterpret_cast<const uint16_t*>(&code[pc + 2]);
        pc += 4;
    } else {
        // MOVE_16
        dst = *reinterpret_cast<const uint16_t*>(&code[pc + 2]);
        src = *reinterpret_cast<const uint16_t*>(&code[pc + 4]);
        pc += 6;
    }
    
    if (dst < ctx.registers.size() && src < ctx.registers.size()) {
        ctx.registers[dst] = ctx.registers[src];
    }
    return true;
}

bool DexInterpreter::handleConst(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    uint8_t op = code[pc];
    
    if (op == static_cast<uint8_t>(DexOp::CONST_4)) {
        uint8_t b = code[pc + 1];
        uint8_t dst = b & 0x0f;
        int8_t val = static_cast<int8_t>((b >> 4) & 0x0f);
        if (val & 0x08) val |= 0xf0;  // Sign extend
        
        if (dst < ctx.registers.size()) {
            ctx.registers[dst].i = val;
        }
        pc += 2;
    } else if (op == static_cast<uint8_t>(DexOp::CONST_16)) {
        uint8_t dst = code[pc + 1];
        int16_t val = *reinterpret_cast<const int16_t*>(&code[pc + 2]);
        
        if (dst < ctx.registers.size()) {
            ctx.registers[dst].i = val;
        }
        pc += 4;
    } else if (op == static_cast<uint8_t>(DexOp::CONST)) {
        uint8_t dst = code[pc + 1];
        int32_t val = *reinterpret_cast<const int32_t*>(&code[pc + 2]);
        
        if (dst < ctx.registers.size()) {
            ctx.registers[dst].i = val;
        }
        pc += 6;
    } else {
        pc += 2;
    }
    
    return true;
}

bool DexInterpreter::handleReturn(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    uint8_t op = code[pc];
    
    if (op == static_cast<uint8_t>(DexOp::RETURN_VOID)) {
        ctx.running = false;
        pc += 2;
    } else {
        uint8_t src = code[pc + 1];
        if (src < ctx.registers.size()) {
            ctx.result = ctx.registers[src];
        }
        ctx.running = false;
        pc += 2;
    }
    
    return true;
}

bool DexInterpreter::handleInvoke(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    uint8_t op = code[pc];
    uint16_t method_idx = *reinterpret_cast<const uint16_t*>(&code[pc + 2]);
    uint16_t args = *reinterpret_cast<const uint16_t*>(&code[pc + 4]);
    
    if (debug_mode_) {
        std::cout << "[DEX] Invoke method " << method_idx << "\n";
    }
    
    // Collect arguments
    std::vector<RegisterValue> invoke_args;
    for (int i = 0; i < ((args >> 4) & 0xf); i++) {
        uint8_t reg = args & 0x0f;
        if (reg < ctx.registers.size()) {
            invoke_args.push_back(ctx.registers[reg]);
        }
    }
    
    // Call method (simplified - would recurse)
    pc += 6;
    return true;
}

bool DexInterpreter::handleIf(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    uint8_t op = code[pc];
    uint8_t a = code[pc + 1] & 0x0f;
    uint8_t b = (code[pc + 1] >> 4) & 0x0f;
    int16_t offset = *reinterpret_cast<const int16_t*>(&code[pc + 2]);
    
    bool condition = false;
    
    if (op == static_cast<uint8_t>(DexOp::IF_EQ)) {
        condition = (ctx.registers[a].i == ctx.registers[b].i);
    } else if (op == static_cast<uint8_t>(DexOp::IF_NE)) {
        condition = (ctx.registers[a].i != ctx.registers[b].i);
    }
    
    if (condition) {
        pc += offset * 2;
    } else {
        pc += 4;
    }
    
    return true;
}

bool DexInterpreter::handleGoto(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    int8_t offset = code[pc + 1];
    pc += offset * 2;
    return true;
}

bool DexInterpreter::handleMath(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    uint8_t op = code[pc];
    uint8_t b = code[pc + 1];
    uint8_t dst = b & 0x0f;
    uint8_t src = (b >> 4) & 0x0f;
    
    if (dst < ctx.registers.size() && src < ctx.registers.size()) {
        int32_t a = ctx.registers[dst].i;
        int32_t b_val = ctx.registers[src].i;
        int32_t result = 0;
        
        if (op == static_cast<uint8_t>(DexOp::ADD_INT_2ADDR)) {
            result = a + b_val;
        } else if (op == static_cast<uint8_t>(DexOp::SUB_INT_2ADDR)) {
            result = a - b_val;
        } else if (op == static_cast<uint8_t>(DexOp::MUL_INT_2ADDR)) {
            result = a * b_val;
        } else if (op == static_cast<uint8_t>(DexOp::DIV_INT_2ADDR)) {
            result = b_val != 0 ? a / b_val : 0;
        }
        
        ctx.registers[dst].i = result;
    }
    
    pc += 2;
    return true;
}

bool DexInterpreter::handleArray(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    // Array operations - simplified stubs
    pc += 4;
    return true;
}

bool DexInterpreter::handleField(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    // Field access - simplified stubs
    pc += 4;
    return true;
}

bool DexInterpreter::handleNew(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    // Object creation - simplified stub
    pc += 4;
    return true;
}

bool DexInterpreter::handleConvert(ExecutionContext& ctx, const uint8_t* code, uint32_t& pc) {
    // Type conversion - simplified stub
    pc += 2;
    return true;
}

// Helper methods
const uint8_t* DexInterpreter::getMethodCode(uint32_t method_idx, size_t& code_size) {
    // This is a simplified implementation
    // Real implementation would look up method in class data items
    code_size = 0;
    return nullptr;
}

bool DexInterpreter::isNativeMethod(uint32_t method_idx) {
    // Check access flags in class data
    return false;  // Simplified
}

ExecutionResult DexInterpreter::callNative(uint32_t method_idx,
                                            const std::vector<RegisterValue>& args) {
    std::cerr << "[!] Native method call not yet implemented\n";
    return ExecutionResult();
}

bool DexInterpreter::resolveMethod(uint32_t method_idx, std::string& class_name,
                                  std::string& method_name, std::string& signature) {
    const auto& methods = dex_.getMethodIds();
    if (method_idx >= methods.size()) return false;
    
    const auto& method = methods[method_idx];
    class_name = dex_.getTypeName(method.class_idx);
    method_name = dex_.getMethodName(method_idx);
    
    // Get signature from proto
    const auto& protos = dex_.getProtoIds();
    if (method.proto_idx < protos.size()) {
        const auto& proto = protos[method.proto_idx];
        signature = dex_.getTypeName(proto.return_type_idx);
    }
    
    return true;
}

void DexInterpreter::dumpState(const ExecutionContext& ctx) {
    std::cout << "=== DEX Interpreter State ===\n";
    std::cout << "PC: " << ctx.pc << "\n";
    std::cout << "Running: " << (ctx.running ? "yes" : "no") << "\n";
    std::cout << "Registers: " << ctx.registers.size() << "\n";
    for (size_t i = 0; i < std::min(ctx.registers.size(), size_t(16)); i++) {
        std::cout << "  r" << i << " = " << ctx.registers[i].i << "\n";
    }
}

} // namespace runtime
} // namespace apkx
