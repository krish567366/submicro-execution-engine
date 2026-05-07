#pragma once

#include "common_types.hpp"
#include <sys/mman.h>
#include <vector>
#include <cstring>
#include <iostream>

namespace hft {
namespace jit {

/**
 * Nano-JIT (Just-In-Time) Compiler
 * 
 * Concept (Ultimate Flexibility):
 * Traditional HFT requires recompilation to change logic (C++).
 * Scripting languages (Python/Lua) are too slow (Interpreters).
 * 
 * Solution:
 * Generate x86-64 Machine Code directly into executable memory at runtime.
 * Allows deploying new "micro-logic" (e.g., "If BidQty > 500 AND Vol < 0.1 Then Buy")
 * in *microseconds* without stopping the engine or recompiling.
 * 
 * Performance:
 * Native execution speed (same as -O3 compiled C++).
 */
class JitAssembler {
public:
    // Simple Register Mapping
    enum Register {
        RAX = 0, RCX = 1, RDX = 2, RBX = 3, 
        RSP = 4, RBP = 5, RSI = 6, RDI = 7
    };

    JitAssembler(size_t capacity = 4096) {
        // Allocate RWX memory (Read-Write-Execute)
        // Note: Modern security (W^X) might require mprotect flipping
        size_ = capacity;
        code_buffer_ = (uint8_t*)mmap(nullptr, size_, 
            PROT_READ | PROT_WRITE | PROT_EXEC, 
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        cursor_ = code_buffer_;
    }

    ~JitAssembler() {
        if (code_buffer_) munmap(code_buffer_, size_);
    }

    // ==== OpCode Emitters ====

    // MOV R1, Imm64 (Move immediate value to register)
    // REX.W + B8+rd io
    void emit_mov_imm(Register reg, uint64_t val) {
        *cursor_++ = 0x48; // REX.W
        *cursor_++ = 0xB8 + reg; // Opcode
        std::memcpy(cursor_, &val, 8);
        cursor_ += 8;
    }

    // CMP R1, R2 (Compare two registers)
    // REX.W + 39 /r
    void emit_cmp_reg(Register r1, Register r2) {
        *cursor_++ = 0x48;
        *cursor_++ = 0x39;
        *cursor_++ = 0xC0 | (r2 << 3) | r1; // ModR/M
    }

    // RET (Return)
    void emit_ret() {
        *cursor_++ = 0xC3;
    }

    // JLE (Jump Less or Equal) - 8-bit relative offset
    // 7E cb
    void emit_jle_rel(int8_t offset) {
        *cursor_++ = 0x7E;
        *cursor_++ = (uint8_t)offset;
    }

    // Function Entry Prologue
    void emit_prologue() {
        // push rbp; mov rbp, rsp
        *cursor_++ = 0x55;
        *cursor_++ = 0x48; *cursor_++ = 0x89; *cursor_++ = 0xE5;
    }

    // Function Exit Epilogue
    void emit_epilogue() {
        // pop rbp; ret
        *cursor_++ = 0x5D;
        *cursor_++ = 0xC3;
    }

    // Finalize and Return Function Pointer
    // Signature: int (*)(uint64_t market_val)
    using FuncPtr = int (*)(uint64_t);
    
    FuncPtr compile() {
        // W^X Security: In prod, we should mprotect(PROT_EXEC | PROT_READ) now
        // to disable WRITE permission.
        return reinterpret_cast<FuncPtr>(code_buffer_);
    }

    void reset() {
        cursor_ = code_buffer_;
    }

private:
    size_t size_;
    uint8_t* code_buffer_;
    uint8_t* cursor_;
};

/**
 * Example Strategy Builder
 * 
 * Generates: "If (Input > Threshold) Return 1 Else Return 0"
 */
class StrategySynthesizer {
public:
    static auto create_threshold_trigger(uint64_t threshold) {
        static JitAssembler jit;
        jit.reset();
        
        // Input logic:
        // Argument 1 (market_val) is in RDI (SysV ABI)
        
        jit.emit_prologue();
        
        // Load threshold into RAX
        jit.emit_mov_imm(JitAssembler::RAX, threshold);
        
        // Compare RDI (Market) with RAX (Threshold)
        // CMP RDI, RAX
        jit.emit_cmp_reg(JitAssembler::RDI, JitAssembler::RAX);
        
        // Note: Real JIT needs label patching. 
        // Here we hardcode offsets for simplicity.
        // JLE +5 bytes (Skip mov eax, 1)
        jit.emit_jle_rel(5); 
        
        // True Case: Return 1
        // MOV RAX, 1 (Optimized: B8 01 00 00 00 is 5 bytes)
        *jit.cursor_++ = 0xB8; 
        uint32_t one = 1;
        std::memcpy(jit.cursor_, &one, 4); jit.cursor_ += 4;
        
        // Jump to End (Skip False Case)
        // JMP +5 (Simple short jump EB 05)
        *jit.cursor_++ = 0xEB; *jit.cursor_++ = 0x05;
        
        // False Case: Return 0 (XOR RAX, RAX is shorter, but let's stick to MOV)
        // Target of JLE
        *jit.cursor_++ = 0xB8;
        uint32_t zero = 0;
        std::memcpy(jit.cursor_, &zero, 4); jit.cursor_ += 4;
        
        // Target of JMP
        jit.emit_epilogue();
        
        return jit.compile();
    }
};

}
}
