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
    std::cout << '\n' << "==== " << name << " ====\n";
    std::cout << "Line    Hexa  ";
    print_tab(1, 4) << "  Byte  ";
    print_tab(1, 4) << "Instruction (For multi byte instructions: hex first, then decimal)\n";
    std::cout << "----  --------";
    print_tab(1, 4) << "--------";
    print_tab(1, 4) << "----------- ------------------------------------------------------\n";
    std::size_t i = 0;
    std::size_t insn_count = 1;
    while (i < chunk.bytes.size()) {
        i += disassemble_instruction(chunk, static_cast<Instruction>(chunk.bytes[i]), i, insn_count);
        insn_count++;
    }
}

std::ostream &print_preamble(Chunk &chunk, std::string_view name, std::size_t byte, std::size_t insn_count) {
    static std::size_t previous_line_number = -1;
    if (std::size_t line_number = chunk.get_line_number(insn_count); line_number == previous_line_number) {
        std::cout << std::setw(4) << std::setfill(' ') << "|"
                  << "  ";
    } else {
        previous_line_number = line_number;
        std::cout << std::setw(4) << std::setfill('0') << chunk.get_line_number(insn_count) << "  ";
    }
    std::cout << std::hex << std::setw(8) << std::setfill('0') << byte;
    std::cout << std::resetiosflags(std::ios_base::hex);
    print_tab(1, 4) << std::setw(8) << byte;
    return print_tab(1, 4) << name;
}

std::size_t single_byte_insn(Chunk &chunk, std::string_view name, std::size_t byte, std::size_t insn_count) {
    print_preamble(chunk, name, byte, insn_count) << '\n';
    return 1;
}

std::size_t two_byte_insn(Chunk &chunk, std::string_view name, std::size_t byte, std::size_t insn_count) {
    print_preamble(chunk, name, byte, insn_count);
    std::size_t next_byte = chunk.bytes[byte + 1];
    if (name == "CONST_SHORT") {
        std::cout << '\t';
        print_tab(1) << "-> " << next_byte << " | value = " << chunk.constants[next_byte].repr() << '\n';
    } else {
        std::cout << "\t-> " << next_byte << '\n';
    }

    for (int i = 1; i < 2; i++) {
        std::size_t offset_bit = chunk.bytes[byte + i];
        print_preamble(chunk, "", byte + i, insn_count) << "| " << std::hex << std::setw(8) << offset_bit;
        print_tab(1, 2) << std::resetiosflags(std::ios_base::hex) << std::setw(8) << offset_bit << '\n';
    }

    return 2;
}

std::size_t four_byte_insn(Chunk &chunk, std::string_view name, std::size_t byte, std::size_t insn_count) {
    print_preamble(chunk, name, byte, insn_count) << '\t';
    std::size_t next_bytes = chunk.bytes[byte + 1];
    next_bytes = (next_bytes << 8) | chunk.bytes[byte + 2];
    next_bytes = (next_bytes << 8) | chunk.bytes[byte + 3];
    if (name == "CONST_LONG") {
        print_tab(1) << "| " << chunk.constants[next_bytes].repr() << '\n';
    } else if (name == "JUMP_FORWARD" || name == "POP_JUMP_IF_FALSE" || name == "JUMP_IF_FALSE") {
        std::cout << "\t| offset = +" << next_bytes << ", jump to = " << byte + next_bytes + 4 << '\n';
    } else if (name == "JUMP_BACKWARD" || name == "POP_JUMP_BACK_IF_TRUE") {
        std::cout << "\t| offset = -" << next_bytes << ", jump to = " << byte + 4 - next_bytes << '\n';
    } else if (name == "ASSIGN_LOCAL") {
        std::cout << "\t| assign to " << next_bytes << '\n';
    } else {
        std::cout << '\n';
    }

    for (int i = 1; i < 4; i++) {
        std::size_t offset_bit = chunk.bytes[byte + i];
        print_preamble(chunk, "", byte + i, insn_count) << "| " << std::hex << std::setw(8) << offset_bit;
        print_tab(1, 2) << std::resetiosflags(std::ios_base::hex) << std::setw(8) << offset_bit << '\n';
    }

    return 4;
}

std::size_t disassemble_instruction(Chunk &chunk, Instruction instruction, std::size_t byte, std::size_t insn_count) {
    switch (instruction) {
        case Instruction::HALT: return single_byte_insn(chunk, "HALT", byte, insn_count);
        case Instruction::POP: return single_byte_insn(chunk, "POP", byte, insn_count);
        case Instruction::CONST_SHORT: return two_byte_insn(chunk, "CONST_SHORT", byte, insn_count);
        case Instruction::CONST_LONG: return four_byte_insn(chunk, "CONST_LONG", byte, insn_count);
        case Instruction::CONCAT: return single_byte_insn(chunk, "CONCAT", byte, insn_count);
        case Instruction::ADD: return single_byte_insn(chunk, "ADD", byte, insn_count);
        case Instruction::SUB: return single_byte_insn(chunk, "SUB", byte, insn_count);
        case Instruction::MUL: return single_byte_insn(chunk, "MUL", byte, insn_count);
        case Instruction::DIV: return single_byte_insn(chunk, "DIV", byte, insn_count);
        case Instruction::MOD: return single_byte_insn(chunk, "MOD", byte, insn_count);
        case Instruction::SHIFT_LEFT: return single_byte_insn(chunk, "SHIFT_LEFT", byte, insn_count);
        case Instruction::SHIFT_RIGHT: return single_byte_insn(chunk, "SHIFT_RIGHT", byte, insn_count);
        case Instruction::BIT_AND: return single_byte_insn(chunk, "BIT_AND", byte, insn_count);
        case Instruction::BIT_OR: return single_byte_insn(chunk, "BIT_OR", byte, insn_count);
        case Instruction::BIT_NOT: return single_byte_insn(chunk, "BIT_NOT", byte, insn_count);
        case Instruction::BIT_XOR: return single_byte_insn(chunk, "BIT_XOR", byte, insn_count);
        case Instruction::NOT: return single_byte_insn(chunk, "NOT", byte, insn_count);
        case Instruction::EQUAL: return single_byte_insn(chunk, "EQUAL", byte, insn_count);
        case Instruction::GREATER: return single_byte_insn(chunk, "GREATER", byte, insn_count);
        case Instruction::LESSER: return single_byte_insn(chunk, "LESSER", byte, insn_count);
        case Instruction::NEGATE: return single_byte_insn(chunk, "NEGATE", byte, insn_count);
        case Instruction::TRUE: return single_byte_insn(chunk, "TRUE", byte, insn_count);
        case Instruction::FALSE: return single_byte_insn(chunk, "FALSE", byte, insn_count);
        case Instruction::NULL_: return single_byte_insn(chunk, "NULL_", byte, insn_count);
        case Instruction::ACCESS_LOCAL_SHORT: return two_byte_insn(chunk, "ACCESS_LOCAL_SHORT", byte, insn_count);
        case Instruction::ACCESS_LOCAL_LONG: return four_byte_insn(chunk, "ACCESS_LOCAL_LONG", byte, insn_count);
        case Instruction::JUMP_FORWARD: return four_byte_insn(chunk, "JUMP_FORWARD", byte, insn_count);
        case Instruction::JUMP_BACKWARD: return four_byte_insn(chunk, "JUMP_BACKWARD", byte, insn_count);
        case Instruction::JUMP_IF_FALSE: return four_byte_insn(chunk, "JUMP_IF_FALSE", byte, insn_count);
        case Instruction::POP_JUMP_IF_FALSE: return four_byte_insn(chunk, "POP_JUMP_IF_FALSE", byte, insn_count);
        case Instruction::POP_JUMP_BACK_IF_TRUE:
            return four_byte_insn(chunk, "POP_JUMP_BACK_IF_TRUE", byte, insn_count);
        case Instruction::ASSIGN_LOCAL: return four_byte_insn(chunk, "ASSIGN_LOCAL", byte, insn_count);
    }
    unreachable();
}