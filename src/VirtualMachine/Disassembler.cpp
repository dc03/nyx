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
        disassemble_instruction(chunk, static_cast<Instruction>(chunk.bytes[i] >> 24), i);
        i++;
    }
}

std::ostream &print_preamble(Chunk &chunk, std::string_view name, std::size_t byte, std::size_t insn_ptr) {
    static std::size_t previous_line_number = -1;
    std::size_t line_number = chunk.get_line_number(insn_ptr);
    if (line_number == previous_line_number) {
        std::cout << std::setw(4) << std::setfill(' ') << "|"
                  << "  ";
    } else {
        previous_line_number = line_number;
        std::cout << std::setw(4) << std::setfill('0') << line_number << "  ";
    }
    std::cout << std::hex << std::setw(8) << std::setfill('0') << byte << std::resetiosflags(std::ios_base::hex);
    print_tab(1, 4) << std::setw(8) << byte;
    return print_tab(1, 4) << name;
}

void instruction(Chunk &chunk, std::string_view name, std::size_t where) {
    print_preamble(chunk, name, where * 4, where + 1);
    std::size_t next_bytes = chunk.bytes[where] & 0x00ff'ffff;

    auto print_trailing_bytes = [&chunk, &where] {
        for (int i = 1; i < 4; i++) {
            std::size_t offset_bit = chunk.bytes[where] & (0xff << (8 * (3 - i)));
            print_preamble(chunk, "", where * 4 + i, where + 1) << "| " << std::hex << std::setw(8) << offset_bit;
            print_tab(1, 2) << std::resetiosflags(std::ios_base::hex) << std::setw(8) << offset_bit << '\n';
        }
    };

    // To avoid polluting the output with unnecessary zeroes, the instruction operand is only printed for specific
    // instructions
    if (name == "CONSTANT" || name == "CONSTANT_STRING") {
        std::cout << "\t\t";
        print_tab(1) << "-> " << next_bytes << " | value = " << chunk.constants[next_bytes].repr() << '\n';
        print_trailing_bytes();
    } else if (name == "JUMP_FORWARD" || name == "POP_JUMP_IF_FALSE" || name == "JUMP_IF_FALSE" ||
               name == "JUMP_IF_TRUE" || name == "POP_JUMP_IF_EQUAL") {
        std::cout << "\t\t| offset = +" << (next_bytes + 1) * 4 << " bytes, jump to = " << 4 * (where + next_bytes + 1)
                  << '\n';
        print_trailing_bytes();
    } else if (name == "JUMP_BACKWARD" || name == "POP_JUMP_BACK_IF_TRUE") {
        std::cout << "\t\t| offset = -" << (next_bytes - 1) * 4 << " bytes, jump to = " << 4 * (where + 1 - next_bytes)
                  << '\n';
        print_trailing_bytes();
    } else if (name == "ASSIGN_LOCAL") {
        std::cout << "\t\t| assign to local " << next_bytes << '\n';
        print_trailing_bytes();
    } else if (name == "ASSIGN_GLOBAL") {
        std::cout << "\t\t| assign to global " << next_bytes << '\n';
        print_trailing_bytes();
    } else if (name == "MAKE_REF_TO_LOCAL") {
        std::cout << "\t\t| make ref to local " << next_bytes << '\n';
        print_trailing_bytes();
    } else if (name == "MAKE_REF_TO_GLOBAL") {
        std::cout << "\t\t| make ref to global " << next_bytes << '\n';
        print_trailing_bytes();
    } else if (name == "ACCESS_LOCAL" || name == "ACCESS_LOCAL_STRING") {
        std::cout << "\t\t| access local " << next_bytes << '\n';
        print_trailing_bytes();
    } else if (name == "ACCESS_GLOBAL" || name == "ACCESS_GLOBAL_STRING") {
        std::cout << "\t\t| access global " << next_bytes << '\n';
        print_trailing_bytes();
    } else if (name == "RETURN") {
        std::cout << "\t\t| pop " << next_bytes << " local(s)\n";
        print_trailing_bytes();
    } else {
        std::cout << '\n';
    }
}

void disassemble_instruction(Chunk &chunk, Instruction insn, std::size_t where) {
    switch (insn) {
        case Instruction::HALT: instruction(chunk, "HALT", where); return;
        case Instruction::POP: instruction(chunk, "POP", where); return;
        case Instruction::CONSTANT: instruction(chunk, "CONSTANT", where); return;
        case Instruction::IADD: instruction(chunk, "IADD", where); return;
        case Instruction::ISUB: instruction(chunk, "ISUB", where); return;
        case Instruction::IMUL: instruction(chunk, "IMUL", where); return;
        case Instruction::IDIV: instruction(chunk, "IDIV", where); return;
        case Instruction::IMOD: instruction(chunk, "IMOD", where); return;
        case Instruction::INEG: instruction(chunk, "INEG", where); return;
        case Instruction::FADD: instruction(chunk, "FADD", where); return;
        case Instruction::FSUB: instruction(chunk, "FSUB", where); return;
        case Instruction::FMUL: instruction(chunk, "FMUL", where); return;
        case Instruction::FDIV: instruction(chunk, "FDIV", where); return;
        case Instruction::FMOD: instruction(chunk, "FMOD", where); return;
        case Instruction::FNEG: instruction(chunk, "FNEG", where); return;
        case Instruction::FLOAT_TO_INT: instruction(chunk, "FLOAT_TO_INT", where); return;
        case Instruction::INT_TO_FLOAT: instruction(chunk, "INT_TO_FLOAT", where); return;
        case Instruction::SHIFT_LEFT: instruction(chunk, "SHIFT_LEFT", where); return;
        case Instruction::SHIFT_RIGHT: instruction(chunk, "SHIFT_RIGHT", where); return;
        case Instruction::BIT_AND: instruction(chunk, "BIT_AND", where); return;
        case Instruction::BIT_OR: instruction(chunk, "BIT_OR", where); return;
        case Instruction::BIT_NOT: instruction(chunk, "BIT_NOT", where); return;
        case Instruction::BIT_XOR: instruction(chunk, "BIT_XOR", where); return;
        case Instruction::NOT: instruction(chunk, "NOT", where); return;
        case Instruction::EQUAL: instruction(chunk, "EQUAL", where); return;
        case Instruction::GREATER: instruction(chunk, "GREATER", where); return;
        case Instruction::LESSER: instruction(chunk, "LESSER", where); return;
        case Instruction::PUSH_TRUE: instruction(chunk, "PUSH_TRUE", where); return;
        case Instruction::PUSH_FALSE: instruction(chunk, "PUSH_FALSE", where); return;
        case Instruction::PUSH_NULL: instruction(chunk, "PUSH_NULL", where); return;
        case Instruction::JUMP_FORWARD: instruction(chunk, "JUMP_FORWARD", where); return;
        case Instruction::JUMP_BACKWARD: instruction(chunk, "JUMP_BACKWARD", where); return;
        case Instruction::JUMP_IF_TRUE: instruction(chunk, "JUMP_IF_TRUE", where); return;
        case Instruction::JUMP_IF_FALSE: instruction(chunk, "JUMP_IF_FALSE", where); return;
        case Instruction::POP_JUMP_IF_EQUAL: instruction(chunk, "POP_JUMP_IF_EQUAL", where); return;
        case Instruction::POP_JUMP_IF_FALSE: instruction(chunk, "POP_JUMP_IF_FALSE", where); return;
        case Instruction::POP_JUMP_BACK_IF_TRUE: instruction(chunk, "POP_JUMP_BACK_IF_TRUE", where); return;
        case Instruction::ASSIGN_LOCAL: instruction(chunk, "ASSIGN_LOCAL", where); return;
        case Instruction::ACCESS_LOCAL: instruction(chunk, "ACCESS_LOCAL", where); return;
        case Instruction::MAKE_REF_TO_LOCAL: instruction(chunk, "MAKE_REF_TO_LOCAL", where); return;
        case Instruction::DEREF: instruction(chunk, "DEREF", where); return;
        case Instruction::ASSIGN_GLOBAL: instruction(chunk, "ASSIGN_GLOBAL", where); return;
        case Instruction::ACCESS_GLOBAL: instruction(chunk, "ACCESS_GLOBAL", where); return;
        case Instruction::MAKE_REF_TO_GLOBAL: instruction(chunk, "MAKE_REF_TO_GLOBAL", where); return;
        case Instruction::LOAD_FUNCTION: instruction(chunk, "LOAD_FUNCTION", where); return;
        case Instruction::CALL_FUNCTION: instruction(chunk, "CALL_FUNCTION", where); return;
        case Instruction::CALL_NATIVE: instruction(chunk, "CALL_NATIVE", where); return;
        case Instruction::RETURN: instruction(chunk, "RETURN", where); return;
        case Instruction::TRAP_RETURN: instruction(chunk, "TRAP_RETURN", where); return;
        case Instruction::CONSTANT_STRING: instruction(chunk, "CONSTANT_STRING", where); return;
        case Instruction::ACCESS_LOCAL_STRING: instruction(chunk, "ACCESS_LOCAL_STRING", where); return;
        case Instruction::ACCESS_GLOBAL_STRING: instruction(chunk, "ACCESS_GLOBAL_STRING", where); return;
        case Instruction::POP_STRING: instruction(chunk, "POP_STRING", where); return;
        case Instruction::CONCATENATE: instruction(chunk, "CONCATENATE", where); return;
        case Instruction::MAKE_LIST: instruction(chunk, "MAKE_LIST", where); return;
        case Instruction::COPY_LIST: instruction(chunk, "COPY_LIST", where); return;
        case Instruction::APPEND_LIST: instruction(chunk, "APPEND_LIST", where); return;
        case Instruction::ASSIGN_LIST: instruction(chunk, "ASSIGN_LIST", where); return;
        case Instruction::INDEX_LIST: instruction(chunk, "INDEX_LIST", where); return;
        case Instruction::CHECK_INDEX: instruction(chunk, "CHECK_INDEX", where); return;
        case Instruction::ASSIGN_LOCAL_LIST: instruction(chunk, "ASSIGN_LOCAL_LIST", where); return;
        case Instruction::ASSIGN_GLOBAL_LIST: instruction(chunk, "ASSIGN_GLOBAL_LIST", where); return;
        case Instruction::POP_LIST: instruction(chunk, "POP_LIST", where); return;
        case Instruction::ACCESS_FROM_TOP: instruction(chunk, "ACCESS_FROM_TOP", where); return;
    }
    unreachable();
}
