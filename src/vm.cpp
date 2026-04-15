#include "apkx/vm.hpp"

#include <iostream>
#include <cstring>

namespace apkx {
namespace runtime {

VM::VM() = default;
VM::~VM() = default;

void VM::pushFrame(std::unique_ptr<Frame> frame) {
    call_stack_.push(std::move(frame));
}

void VM::popFrame() {
    if (!call_stack_.empty()) {
        call_stack_.pop();
    }
}

Frame* VM::currentFrame() {
    if (call_stack_.empty()) return nullptr;
    return call_stack_.top().get();
}

Object* VM::allocateObject(uint32_t class_idx) {
    void* id = next_obj_id_;
    next_obj_id_ = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(next_obj_id_) + 1);
    
    heap_[id] = std::make_unique<Object>(class_idx);
    return heap_[id].get();
}

Object* VM::getObject(void* ref) {
    auto it = heap_.find(ref);
    if (it != heap_.end()) {
        return it->second.get();
    }
    return nullptr;
}

ExecutionResult VM::execute(const uint8_t* code, size_t code_size) {
    if (!code || code_size == 0) {
        return ExecutionResult();
    }
    
    Frame* frame = currentFrame();
    if (!frame) {
        return ExecutionResult();
    }
    
    while (frame->pc < code_size) {
        auto result = executeInstruction(code, frame->pc);
        if (!result.success) {
            return result;
        }
        
        // Check for exception
        if (hasPendingException()) {
            return ExecutionResult();
        }
    }
    
    return ExecutionResult(RegisterValue());
}

ExecutionResult VM::executeInstruction(const uint8_t* code, uint32_t& pc) {
    uint8_t opcode = code[pc];
    
    switch (static_cast<OpCode>(opcode)) {
        case OpCode::NOP:
            pc += 2;
            break;
            
        case OpCode::MOVE: {
            uint8_t dst = code[pc + 1] & 0x0f;
            uint8_t src = (code[pc + 1] >> 4) & 0x0f;
            opMove(dst, src);
            pc += 2;
            break;
        }
        
        case OpCode::CONST_4: {
            uint8_t dst = code[pc + 1] & 0x0f;
            int8_t val = static_cast<int8_t>((code[pc + 1] >> 4) & 0x0f);
            if (val & 0x08) val |= 0xf0;  // Sign extend
            opConst4(dst, val);
            pc += 2;
            break;
        }
        
        case OpCode::CONST_16: {
            uint8_t dst = code[pc + 1];
            int16_t val = *reinterpret_cast<const int16_t*>(&code[pc + 2]);
            opConst16(dst, val);
            pc += 4;
            break;
        }
        
        case OpCode::RETURN_VOID:
            return ExecutionResult(RegisterValue());
            
        case OpCode::RETURN: {
            uint8_t src = code[pc + 1];
            Frame* frame = currentFrame();
            RegisterValue result;
            if (frame && src < frame->registers.size()) {
                result = frame->registers[src];
            }
            return ExecutionResult(result);
        }
        
        case OpCode::GOTO: {
            int16_t offset = *reinterpret_cast<const int16_t*>(&code[pc + 2]);
            pc += offset * 2;
            break;
        }
        
        default:
            std::cerr << "[!] Unimplemented opcode: 0x" 
                      << std::hex << static_cast<int>(opcode) << std::dec 
                      << " at pc=" << pc << "\n";
            pc += 2;  // Skip unknown
            break;
    }
    
    return ExecutionResult(RegisterValue());
}

// Opcode implementations
void VM::opMove(uint8_t dst, uint8_t src) {
    Frame* frame = currentFrame();
    if (frame && dst < frame->registers.size() && 
        src < frame->registers.size()) {
        frame->registers[dst] = frame->registers[src];
    }
}

void VM::opConst4(uint8_t dst, int8_t value) {
    Frame* frame = currentFrame();
    if (frame && dst < frame->registers.size()) {
        frame->registers[dst].i = value;
    }
}

void VM::opConst16(uint8_t dst, int16_t value) {
    Frame* frame = currentFrame();
    if (frame && dst < frame->registers.size()) {
        frame->registers[dst].i = value;
    }
}

void VM::throwException(const std::string& class_name, 
                        const std::string& msg) {
    exception_pending_ = true;
    exception_class_ = class_name;
    exception_msg_ = msg;
    std::cerr << "[!] Exception: " << class_name << ": " << msg << "\n";
}

bool VM::hasPendingException() const {
    return exception_pending_;
}

void VM::clearException() {
    exception_pending_ = false;
    exception_class_.clear();
    exception_msg_.clear();
}

void VM::dumpState() {
    std::cout << "=== VM State ===\n";
    std::cout << "Call stack depth: " << call_stack_.size() << "\n";
    std::cout << "Heap objects: " << heap_.size() << "\n";
    
    Frame* frame = currentFrame();
    if (frame) {
        std::cout << "Current frame registers: " << frame->registers.size() << "\n";
        std::cout << "PC: " << frame->pc << "\n";
    }
}

} // namespace runtime
} // namespace apkx
