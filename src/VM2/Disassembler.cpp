/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Disassembler.hpp"

#include "../Common.hpp"
#include "Value.hpp"

#include <iomanip>
#include <iostream>

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
    while (i < chunk.bytes.size()) {
        i += disassemble_instruction(chunk, static_cast<Instruction>(chunk.bytes[i]), i);
    }
}

std::ostream &print_preamble(Chunk &chunk, std::string_view name, std::size_t byte, std::size_t insn_ptr) {
    static std::size_t previous_line_number = -1;
    if (std::size_t line_number = chunk.get_line_number(insn_ptr); line_number == previous_line_number) {
        std::cout << std::setw(4) << std::setfill(' ') << "|"
                  << "  ";
    } else {
        previous_line_number = line_number;
        std::cout << std::setw(4) << std::setfill('0') << chunk.get_line_number(insn_ptr) << "  ";
    }
    std::cout << std::hex << std::setw(8) << std::setfill('0') << byte;
    std::cout << std::resetiosflags(std::ios_base::hex);
    print_tab(1, 4) << std::setw(8) << byte;
    return print_tab(1, 4) << name;
}

std::size_t single_byte_insn(Chunk &chunk, std::string_view name, std::size_t byte) {
    print_preamble(chunk, name, byte, byte + 1) << '\n';
    return 1;
}

std::size_t two_byte_insn(Chunk &chunk, std::string_view name, std::size_t byte) {
    print_preamble(chunk, name, byte, byte + 1);
    std::size_t next_byte = chunk.bytes[byte + 1];
    if (name == "MAKE_LIST_OF") {
        std::cout << '\t';
        print_tab(1) << "| count: " << next_byte << '\n';
    } else {
        std::cout << "\t-> " << next_byte << '\n';
    }

    std::size_t offset_bit = chunk.bytes[byte + 1];
    print_preamble(chunk, "", byte + 1, byte + 1) << "| " << std::hex << std::setw(8) << offset_bit;
    print_tab(1, 2) << std::resetiosflags(std::ios_base::hex) << std::setw(8) << offset_bit << '\n';

    return 2;
}

std::size_t four_byte_insn(Chunk &chunk, std::string_view name, std::size_t byte) {
    print_preamble(chunk, name, byte, byte + 1) << '\t';
    std::size_t next_bytes = chunk.bytes[byte + 1];
    next_bytes = (next_bytes << 8) | chunk.bytes[byte + 2];
    next_bytes = (next_bytes << 8) | chunk.bytes[byte + 3];
    if (name == "CONSTANT" || name == "CONSTANT_STRING") {
        std::cout << '\t';
        print_tab(1) << "-> " << next_bytes << " | value = " << chunk.constants[next_bytes].repr() << '\n';
    } else if (name == "JUMP_FORWARD" || name == "POP_JUMP_IF_FALSE" || name == "JUMP_IF_FALSE" ||
               name == "JUMP_IF_TRUE" || name == "POP_JUMP_IF_EQUAL") {
        std::cout << "\t| offset = +" << next_bytes << ", jump to = " << byte + next_bytes + 4 << '\n';
    } else if (name == "JUMP_BACKWARD" || name == "POP_JUMP_BACK_IF_TRUE") {
        std::cout << "\t| offset = -" << next_bytes << ", jump to = " << byte + 4 - next_bytes << '\n';
    } else if (name == "ASSIGN_LOCAL") {
        std::cout << "\t| assign to local " << next_bytes << '\n';
    } else if (name == "ASSIGN_GLOBAL") {
        std::cout << "\t| assign to global " << next_bytes << '\n';
    } else if (name == "MAKE_REF_TO_LOCAL") {
        std::cout << "\t| make ref to local " << next_bytes << '\n';
    } else if (name == "MAKE_REF_TO_GLOBAL") {
        std::cout << "\t| make ref to global " << next_bytes << '\n';
    } else if (name == "INCR_LOCAL" || name == "DECR_LOCAL" || name == "MUL_LOCAL" || name == "DIV_LOCAL" ||
               name == "INCR_GLOBAL" || name == "DECR_GLOBAL" || name == "MUL_GLOBAL" || name == "DIV_GLOBAL") {
        print_tab(1) << "| modify " << next_bytes << '\n';
    } else if (name == "RETURN") {
        std::cout << "\t| pop " << next_bytes << " local(s)\n";
    } else {
        std::cout << '\n';
    }

    for (int i = 1; i < 4; i++) {
        std::size_t offset_bit = chunk.bytes[byte + i];
        print_preamble(chunk, "", byte + i, byte + 1) << "| " << std::hex << std::setw(8) << offset_bit;
        print_tab(1, 2) << std::resetiosflags(std::ios_base::hex) << std::setw(8) << offset_bit << '\n';
    }

    return 4;
}

std::size_t disassemble_instruction(Chunk &chunk, Instruction instruction, std::size_t byte) {
    switch (instruction) {
        case Instruction::HALT: return single_byte_insn(chunk, "HALT", byte);
        case Instruction::POP: return single_byte_insn(chunk, "POP", byte);
        case Instruction::CONSTANT: return four_byte_insn(chunk, "CONSTANT", byte);
        case Instruction::IADD: return single_byte_insn(chunk, "IADD", byte);
        case Instruction::ISUB: return single_byte_insn(chunk, "ISUB", byte);
        case Instruction::IMUL: return single_byte_insn(chunk, "IMUL", byte);
        case Instruction::IDIV: return single_byte_insn(chunk, "IDIV", byte);
        case Instruction::IMOD: return single_byte_insn(chunk, "IMOD", byte);
        case Instruction::INEG: return single_byte_insn(chunk, "INEG", byte);
        case Instruction::FADD: return single_byte_insn(chunk, "FADD", byte);
        case Instruction::FSUB: return single_byte_insn(chunk, "FSUB", byte);
        case Instruction::FMUL: return single_byte_insn(chunk, "FMUL", byte);
        case Instruction::FDIV: return single_byte_insn(chunk, "FDIV", byte);
        case Instruction::FMOD: return single_byte_insn(chunk, "FMOD", byte);
        case Instruction::FNEG: return single_byte_insn(chunk, "FNEG", byte);
        case Instruction::FLOAT_TO_INT: return single_byte_insn(chunk, "FLOAT_TO_INT", byte);
        case Instruction::INT_TO_FLOAT: return single_byte_insn(chunk, "INT_TO_FLOAT", byte);
        case Instruction::SHIFT_LEFT: return single_byte_insn(chunk, "SHIFT_LEFT", byte);
        case Instruction::SHIFT_RIGHT: return single_byte_insn(chunk, "SHIFT_RIGHT", byte);
        case Instruction::BIT_AND: return single_byte_insn(chunk, "BIT_AND", byte);
        case Instruction::BIT_OR: return single_byte_insn(chunk, "BIT_OR", byte);
        case Instruction::BIT_NOT: return single_byte_insn(chunk, "BIT_NOT", byte);
        case Instruction::BIT_XOR: return single_byte_insn(chunk, "BIT_XOR", byte);
        case Instruction::NOT: return single_byte_insn(chunk, "NOT", byte);
        case Instruction::EQUAL: return single_byte_insn(chunk, "EQUAL", byte);
        case Instruction::GREATER: return single_byte_insn(chunk, "GREATER", byte);
        case Instruction::LESSER: return single_byte_insn(chunk, "LESSER", byte);
        case Instruction::PUSH_TRUE: return single_byte_insn(chunk, "PUSH_TRUE", byte);
        case Instruction::PUSH_FALSE: return single_byte_insn(chunk, "PUSH_FALSE", byte);
        case Instruction::PUSH_NULL: return single_byte_insn(chunk, "PUSH_NULL", byte);
        case Instruction::JUMP_FORWARD: return four_byte_insn(chunk, "JUMP_FORWARD", byte);
        case Instruction::JUMP_BACKWARD: return four_byte_insn(chunk, "JUMP_BACKWARD", byte);
        case Instruction::JUMP_IF_TRUE: return four_byte_insn(chunk, "JUMP_IF_TRUE", byte);
        case Instruction::JUMP_IF_FALSE: return four_byte_insn(chunk, "JUMP_IF_FALSE", byte);
        case Instruction::POP_JUMP_IF_EQUAL: return four_byte_insn(chunk, "POP_JUMP_IF_EQUAL", byte);
        case Instruction::POP_JUMP_IF_FALSE: return four_byte_insn(chunk, "POP_JUMP_IF_FALSE", byte);
        case Instruction::POP_JUMP_BACK_IF_TRUE: return four_byte_insn(chunk, "POP_JUMP_BACK_IF_TRUE", byte);
        case Instruction::ASSIGN_LOCAL: return four_byte_insn(chunk, "ASSIGN_LOCAL", byte);
        case Instruction::ACCESS_LOCAL: return four_byte_insn(chunk, "ACCESS_LOCAL", byte);
        case Instruction::MAKE_REF_TO_LOCAL: return four_byte_insn(chunk, "MAKE_REF_TO_LOCAL", byte);
        case Instruction::DEREF: return single_byte_insn(chunk, "DEREF", byte);
        case Instruction::ASSIGN_GLOBAL: return four_byte_insn(chunk, "ASSIGN_GLOBAL", byte);
        case Instruction::ACCESS_GLOBAL: return four_byte_insn(chunk, "ACCESS_GLOBAL", byte);
        case Instruction::MAKE_REF_TO_GLOBAL: return four_byte_insn(chunk, "MAKE_REF_TO_GLOBAL", byte);
        case Instruction::LOAD_FUNCTION: return single_byte_insn(chunk, "LOAD_FUNCTION", byte);
        case Instruction::CALL_FUNCTION: return single_byte_insn(chunk, "CALL_FUNCTION", byte);
        case Instruction::CALL_NATIVE: return single_byte_insn(chunk, "CALL_NATIVE", byte);
        case Instruction::RETURN: return four_byte_insn(chunk, "RETURN", byte);
        case Instruction::TRAP_RETURN: return single_byte_insn(chunk, "TRAP_RETURN", byte);
        case Instruction::COPY: return single_byte_insn(chunk, "COPY", byte);
        case Instruction::CONSTANT_STRING: return four_byte_insn(chunk, "CONSTANT_STRING", byte);
        case Instruction::ACCESS_LOCAL_STRING: return four_byte_insn(chunk, "ACCESS_LOCAL_STRING", byte);
        case Instruction::ACCESS_GLOBAL_STRING: return four_byte_insn(chunk, "ACCESS_GLOBAL_STRING", byte);
        case Instruction::POP_STRING: return single_byte_insn(chunk, "POP_STRING", byte);
        case Instruction::CONCATENATE: return single_byte_insn(chunk, "CONCATENATE", byte);
        case Instruction::MAKE_LIST: return single_byte_insn(chunk, "MAKE_LIST", byte);
        case Instruction::APPEND_LIST: return single_byte_insn(chunk, "APPEND_LIST", byte);
        case Instruction::ASSIGN_LIST: return single_byte_insn(chunk, "ASSIGN_LIST", byte);
        case Instruction::INDEX_LIST: return single_byte_insn(chunk, "INDEX_LIST", byte);
        case Instruction::CHECK_INDEX: return single_byte_insn(chunk, "CHECK_INDEX", byte);
        case Instruction::ASSIGN_LOCAL_LIST: return four_byte_insn(chunk, "ASSIGN_LOCAL_LIST", byte);
        case Instruction::ASSIGN_GLOBAL_LIST: return four_byte_insn(chunk, "ASSIGN_GLOBAL_LIST", byte);
        case Instruction::DESTROY: return single_byte_insn(chunk, "DESTROY", byte);
    }
    unreachable();
}
