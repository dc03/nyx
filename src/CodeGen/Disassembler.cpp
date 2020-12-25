/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Disassembler.hpp"
#include "../Common.hpp"

#include <iomanip>

std::ostream &print_tab(std::size_t quantity, std::size_t tab_size = 8) {
    for (std::size_t i = 0; i < quantity * tab_size; i++) {
        std::cout << ' ';
    }
    return std::cout;
}

void disassemble(Chunk &chunk, std::string_view name) {
    std::cout << "==== " << name << " ====\n";
    std::cout << "  Hexa  ";
    print_tab(1, 4) << "  Byte  ";
    print_tab(1, 4) << "Instruction (For multi byte instructions: hex first, then decimal)\n";
    std::cout << "--------";
    print_tab(1, 4) << "--------";
    print_tab(1, 4) << "----------- ------------------------------------------------------\n";
    std::size_t i = 0;
    while (i < chunk.bytes.size()) {
        i += disassemble_instruction(chunk, static_cast<Instruction>(chunk.bytes[i]), i);
    }
}

std::ostream &print_preamble(std::string_view name, std::size_t byte) {
    std::cout << std::hex << std::setw(8) << std::setfill('0') << byte;
    std::cout << std::resetiosflags(std::ios_base::hex);
    print_tab(1, 4) << std::setw(8) << byte;
    return print_tab(1, 4) << name;
}

std::size_t single_byte_insn(std::string_view name, std::size_t byte) {
    print_preamble(name, byte) << '\n';
    return 1;
}

std::size_t two_byte_insn(Chunk &chunk, std::string_view name, std::size_t byte) {
    print_preamble(name, byte);
    std::size_t next_byte = chunk.bytes[byte + 1];
    if (name == "CONST_SHORT") {
        std::cout << '\t';
        print_tab(1) << "-> " << next_byte << " | value = " << chunk.constants[next_byte].repr() << '\n';
    } else {
        std::cout << "\t-> " << next_byte << '\n';
    }

    for (int i = 1; i < 2; i++) {
        std::size_t offset_bit = chunk.bytes[byte + i];
        print_preamble("", byte + i) << "| " << std::hex << std::setw(8) << offset_bit;
        print_tab(1, 2) << std::resetiosflags(std::ios_base::hex) << std::setw(8) << offset_bit << '\n';
    }

    return 2;
}

std::size_t four_byte_insn(Chunk &chunk, std::string_view name, std::size_t byte) {
    print_preamble(name, byte) << '\t';
    std::size_t next_bytes = chunk.bytes[byte + 1];
    next_bytes = (next_bytes << 8) | chunk.bytes[byte + 2];
    next_bytes = (next_bytes << 8) | chunk.bytes[byte + 3];
    if (name == "CONST_LONG") {
        print_tab(1) << "| " << chunk.constants[next_bytes].repr() << '\n';
    } else if (name == "JUMP_FORWARD" || name == "JUMP_IF_FALSE") {
        std::cout << "\t| offset = +" << next_bytes << ", jump to = " << byte + next_bytes << '\n';
    } else if (name == "JUMP_BACKWARD") {
        std::cout << "\t| offset = -" << next_bytes << ", jump to = " << byte + 4 - next_bytes << '\n';
    } else if (name == "ASSIGN_LOCAL") {
        std::cout << "\t| assign to " << next_bytes << '\n';
    } else {
        std::cout << '\n';
    }

    for (int i = 1; i < 4; i++) {
        std::size_t offset_bit = chunk.bytes[byte + i];
        print_preamble("", byte + i) << "| " << std::hex << std::setw(8) << offset_bit;
        print_tab(1, 2) << std::resetiosflags(std::ios_base::hex) << std::setw(8) << offset_bit << '\n';
    }

    return 4;
}

std::size_t disassemble_instruction(Chunk &chunk, Instruction instruction, std::size_t byte) {
    switch (instruction) {
        case Instruction::HALT: return single_byte_insn("HALT", byte);
        case Instruction::POP: return single_byte_insn("POP", byte);
        case Instruction::CONST_SHORT: return two_byte_insn(chunk, "CONST_SHORT", byte);
        case Instruction::CONST_LONG: return four_byte_insn(chunk, "CONST_LONG", byte);
        case Instruction::CONCAT: return single_byte_insn("CONCAT", byte);
        case Instruction::ADD: return single_byte_insn("ADD", byte);
        case Instruction::SUB: return single_byte_insn("SUB", byte);
        case Instruction::MUL: return single_byte_insn("MUL", byte);
        case Instruction::DIV: return single_byte_insn("DIV", byte);
        case Instruction::MOD: return single_byte_insn("MOD", byte);
        case Instruction::SHIFT_LEFT: return single_byte_insn("SHIFT_LEFT", byte);
        case Instruction::SHIFT_RIGHT: return single_byte_insn("SHIFT_RIGHT", byte);
        case Instruction::BIT_AND: return single_byte_insn("BIT_AND", byte);
        case Instruction::BIT_OR: return single_byte_insn("BIT_OR", byte);
        case Instruction::BIT_NOT: return single_byte_insn("BIT_NOT", byte);
        case Instruction::BIT_XOR: return single_byte_insn("BIT_XOR", byte);
        case Instruction::NOT: return single_byte_insn("NOT", byte);
        case Instruction::EQUAL: return single_byte_insn("EQUAL", byte);
        case Instruction::GREATER: return single_byte_insn("GREATER", byte);
        case Instruction::LESSER: return single_byte_insn("LESSER", byte);
        case Instruction::NEGATE: return single_byte_insn("NEGATE", byte);
        case Instruction::TRUE: return single_byte_insn("TRUE", byte);
        case Instruction::FALSE: return single_byte_insn("FALSE", byte);
        case Instruction::NULL_: return single_byte_insn("NULL_", byte);
        case Instruction::ACCESS_LOCAL_SHORT: return two_byte_insn(chunk, "ACCESS_LOCAL_SHORT", byte);
        case Instruction::ACCESS_LOCAL_LONG: return four_byte_insn(chunk, "ACCESS_LOCAL_LONG", byte);
        case Instruction::JUMP_FORWARD: return four_byte_insn(chunk, "JUMP_FORWARD", byte);
        case Instruction::JUMP_BACKWARD: return four_byte_insn(chunk, "JUMP_BACKWARD", byte);
        case Instruction::JUMP_IF_FALSE: return four_byte_insn(chunk, "JUMP_IF_FALSE", byte);
        case Instruction::ASSIGN_LOCAL: return four_byte_insn(chunk, "ASSIGN_LOCAL", byte);
    }
    unreachable();
}