/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Backend/VirtualMachine/Disassembler.hpp"

#include "Backend/VirtualMachine/Value.hpp"
#include "ColoredPrintHelper.hpp"
#include "Common.hpp"

#include <iomanip>
#include <iostream>
#include <termcolor/termcolor.hpp>

std::ostream &print_tab(std::size_t quantity, std::size_t tab_size = 8) {
    for (std::size_t i = 0; i < quantity * tab_size; i++) {
        std::cout << ' ';
    }
    return std::cout;
}

ColoredPrintHelper pcife(bool colors_enabled, ColoredPrintHelper::StreamColorModifier colorizer) {
    return ColoredPrintHelper{colors_enabled, colorizer};
}

void disassemble_ctx(BackendContext *ctx, bool colors_enabled) {
    if (ctx->main != nullptr) {
        std::cout << pcife(colors_enabled, termcolor::bold) << pcife(colors_enabled, termcolor::blue)
                  << "\n--<==== Main Module ====>--" << pcife(colors_enabled, termcolor::reset);
        disassemble_module(ctx->main, colors_enabled);
    }

    for (RuntimeModule &module : ctx->compiled_modules) {
        disassemble_module(&module, colors_enabled);
    }
}

void disassemble_module(RuntimeModule *module, bool colors_enabled) {
    std::cout << pcife(colors_enabled, termcolor::cyan) << "\n-<==== Module : " << module->name << " ====>-\n"
              << pcife(colors_enabled, termcolor::reset);
    disassemble_chunk(module->top_level_code, module->name, "<top-level-code>", colors_enabled);
    disassemble_chunk(module->teardown_code, module->name, "<tear-down-code>", colors_enabled);
    for (auto &[name, function] : module->functions) {
        disassemble_chunk(function.code, module->name, name, colors_enabled);
    }
}

void disassemble_chunk(Chunk &chunk, std::string_view module_name, std::string_view name, bool colors_enabled) {
    std::cout << pcife(colors_enabled, termcolor::green) << "\n==== " << pcife(colors_enabled, termcolor::bold)
              << module_name << "$" << name << pcife(colors_enabled, termcolor::reset)
              << pcife(colors_enabled, termcolor::green) << " ====\n"
              << pcife(colors_enabled, termcolor::reset);
    std::cout << pcife(colors_enabled, termcolor::red) << "Line    Hexa  ";
    print_tab(1, 4) << "  Byte  ";
    print_tab(1, 4) << "Instruction\n" << pcife(colors_enabled, termcolor::reset);
    std::cout << pcife(colors_enabled, termcolor::yellow) << "----  --------";
    print_tab(1, 4) << "--------";
    print_tab(1, 4) << "-----------\n" << pcife(colors_enabled, termcolor::reset);
    std::size_t i = 0;
    while (i < chunk.bytes.size()) {
        disassemble_instruction(chunk, static_cast<Instruction>(chunk.bytes[i] >> 24), i, colors_enabled);
        i++;
    }
}

std::ostream &print_preamble(
    Chunk &chunk, std::string_view name, std::size_t byte, std::size_t insn_ptr, bool colors_enabled) {
    static std::size_t previous_line_number = -1;
    std::size_t line_number = chunk.get_line_number(insn_ptr);
    if (line_number == previous_line_number) {
        std::cout << pcife(colors_enabled, termcolor::cyan) << std::setw(4) << std::setfill(' ') << "|"
                  << "  " << pcife(colors_enabled, termcolor::reset);
    } else {
        previous_line_number = line_number;
        std::cout << pcife(colors_enabled, termcolor::cyan) << std::setw(4) << std::setfill('0') << line_number << "  "
                  << pcife(colors_enabled, termcolor::reset);
    }
    std::cout << pcife(colors_enabled, termcolor::blue) << std::hex << std::setw(8) << std::setfill('0') << byte
              << std::resetiosflags(std::ios_base::hex);
    print_tab(1, 4) << pcife(colors_enabled, termcolor::green) << std::setw(8) << byte;
    return print_tab(1, 4) << pcife(colors_enabled, termcolor::bold) << pcife(colors_enabled, termcolor::red) << name
                           << pcife(colors_enabled, termcolor::reset);
}

void instruction(Chunk &chunk, std::string_view name, std::size_t where, bool colors_enabled) {
    print_preamble(chunk, name, where * 4, where + 1, colors_enabled);
    std::size_t next_bytes = chunk.bytes[where] & 0x00ff'ffff;

    auto print_trailing_bytes = [&chunk, &where, &colors_enabled] {
        for (int i = 1; i < 4; i++) {
            std::size_t offset_bit = chunk.bytes[where] & (0xff << (8 * (3 - i)));
            print_preamble(chunk, "", where * 4 + i, where + 1, colors_enabled)
                << pcife(colors_enabled, termcolor::cyan) << "| " << pcife(colors_enabled, termcolor::blue) << std::hex
                << std::setw(8) << offset_bit;
            print_tab(1, 2) << pcife(colors_enabled, termcolor::green) << std::resetiosflags(std::ios_base::hex)
                            << std::setw(8) << offset_bit << pcife(colors_enabled, termcolor::reset) << '\n';
        }
    };

#define PYEL pcife(colors_enabled, termcolor::yellow)
#define PBLU pcife(colors_enabled, termcolor::blue)
#define PRES pcife(colors_enabled, termcolor::reset)

    // To avoid polluting the output with unnecessary zeroes, the instruction operand is only printed for specific
    // instructions
    if (name == "CONSTANT" || name == "CONSTANT_STRING") {
        std::cout << "\t\t";
        print_tab(1) << PYEL << "-> " << next_bytes << " | value = " << PBLU << chunk.constants[next_bytes].repr()
                     << '\n'
                     << PRES;
        print_trailing_bytes();
    } else if (name == "JUMP_FORWARD" || name == "POP_JUMP_IF_FALSE" || name == "JUMP_IF_FALSE" ||
               name == "JUMP_IF_TRUE" || name == "POP_JUMP_IF_EQUAL") {
        std::cout << PYEL << "\t\t| offset = " << PBLU << "+" << (next_bytes + 1) * 4 << PYEL
                  << " bytes, jump to = " << PBLU << 4 * (where + next_bytes + 1) << PRES << '\n';
        print_trailing_bytes();
    } else if (name == "JUMP_BACKWARD" || name == "POP_JUMP_BACK_IF_TRUE") {
        std::cout << PYEL << "\t\t| offset = " << PBLU << "-" << (next_bytes - 1) * 4 << PYEL
                  << " bytes, jump to = " << PBLU << 4 * (where + 1 - next_bytes) << PRES << '\n';
        print_trailing_bytes();
    } else if (name == "ASSIGN_LOCAL") {
        std::cout << PYEL << "\t\t| assign to local " << PBLU << next_bytes << PRES << '\n';
        print_trailing_bytes();
    } else if (name == "ASSIGN_GLOBAL") {
        std::cout << PYEL << "\t\t| assign to global " << PBLU << next_bytes << PRES << '\n';
        print_trailing_bytes();
    } else if (name == "MAKE_REF_TO_LOCAL") {
        std::cout << PYEL << "\t\t| make ref to local " << PBLU << next_bytes << PRES << '\n';
        print_trailing_bytes();
    } else if (name == "MAKE_REF_TO_GLOBAL") {
        std::cout << PYEL << "\t\t| make ref to global " << PBLU << next_bytes << PRES << '\n';
        print_trailing_bytes();
    } else if (name == "ACCESS_LOCAL" || name == "ACCESS_LOCAL_LIST") {
        std::cout << PYEL << "\t\t| access local " << PBLU << next_bytes << PRES << '\n';
        print_trailing_bytes();
    } else if (name == "ACCESS_GLOBAL" || name == "ACCESS_GLOBAL_LIST") {
        std::cout << PYEL << "\t\t| access global " << PBLU << next_bytes << PRES << '\n';
        print_trailing_bytes();
    } else if (name == "ACCESS_FROM_TOP") {
        std::cout << PYEL << "\t\t| access " << PBLU << next_bytes << PYEL << " from top\n" << PRES;
        print_trailing_bytes();
    } else if (name == "ASSIGN_FROM_TOP") {
        std::cout << PYEL << "\t\t| assign " << PBLU << next_bytes << PYEL << " from top\n" << PRES;
        print_trailing_bytes();
    } else if (name == "RETURN") {
        std::cout << PYEL << "\t\t| pop " << PBLU << next_bytes << PYEL << " local(s)\n" << PRES;
        print_trailing_bytes();
    } else if (name == "SWAP") {
        std::cout << PYEL << "\t\t| swap " << PBLU << next_bytes << PYEL << " and " << PBLU << next_bytes + 1 << PYEL
                  << " from top\n"
                  << PRES;
        print_trailing_bytes();
    } else if (name == "MAKE_LIST") {
        std::cout << PYEL << "\t\t| size " << PBLU << next_bytes << "\n";
        print_trailing_bytes();
    } else if (name == "LOAD_FUNCTION_MODULE_INDEX") {
        std::cout << PYEL << "\t\t| module index " << PBLU << next_bytes << "\n";
        print_trailing_bytes();
    } else {
        std::cout << '\n';
    }

#undef PRES
#undef PBLU
#undef PYEL
}

void disassemble_instruction(Chunk &chunk, Instruction insn, std::size_t where, bool colors_enabled) {
    switch (insn) {
        case Instruction::HALT: instruction(chunk, "HALT", where, colors_enabled); return;
        case Instruction::POP: instruction(chunk, "POP", where, colors_enabled); return;
        case Instruction::CONSTANT: instruction(chunk, "CONSTANT", where, colors_enabled); return;
        case Instruction::IADD: instruction(chunk, "IADD", where, colors_enabled); return;
        case Instruction::ISUB: instruction(chunk, "ISUB", where, colors_enabled); return;
        case Instruction::IMUL: instruction(chunk, "IMUL", where, colors_enabled); return;
        case Instruction::IDIV: instruction(chunk, "IDIV", where, colors_enabled); return;
        case Instruction::IMOD: instruction(chunk, "IMOD", where, colors_enabled); return;
        case Instruction::INEG: instruction(chunk, "INEG", where, colors_enabled); return;
        case Instruction::FADD: instruction(chunk, "FADD", where, colors_enabled); return;
        case Instruction::FSUB: instruction(chunk, "FSUB", where, colors_enabled); return;
        case Instruction::FMUL: instruction(chunk, "FMUL", where, colors_enabled); return;
        case Instruction::FDIV: instruction(chunk, "FDIV", where, colors_enabled); return;
        case Instruction::FMOD: instruction(chunk, "FMOD", where, colors_enabled); return;
        case Instruction::FNEG: instruction(chunk, "FNEG", where, colors_enabled); return;
        case Instruction::FLOAT_TO_INT: instruction(chunk, "FLOAT_TO_INT", where, colors_enabled); return;
        case Instruction::INT_TO_FLOAT: instruction(chunk, "INT_TO_FLOAT", where, colors_enabled); return;
        case Instruction::SHIFT_LEFT: instruction(chunk, "SHIFT_LEFT", where, colors_enabled); return;
        case Instruction::SHIFT_RIGHT: instruction(chunk, "SHIFT_RIGHT", where, colors_enabled); return;
        case Instruction::BIT_AND: instruction(chunk, "BIT_AND", where, colors_enabled); return;
        case Instruction::BIT_OR: instruction(chunk, "BIT_OR", where, colors_enabled); return;
        case Instruction::BIT_NOT: instruction(chunk, "BIT_NOT", where, colors_enabled); return;
        case Instruction::BIT_XOR: instruction(chunk, "BIT_XOR", where, colors_enabled); return;
        case Instruction::NOT: instruction(chunk, "NOT", where, colors_enabled); return;
        case Instruction::EQUAL: instruction(chunk, "EQUAL", where, colors_enabled); return;
        case Instruction::GREATER: instruction(chunk, "GREATER", where, colors_enabled); return;
        case Instruction::LESSER: instruction(chunk, "LESSER", where, colors_enabled); return;
        case Instruction::PUSH_TRUE: instruction(chunk, "PUSH_TRUE", where, colors_enabled); return;
        case Instruction::PUSH_FALSE: instruction(chunk, "PUSH_FALSE", where, colors_enabled); return;
        case Instruction::PUSH_NULL: instruction(chunk, "PUSH_NULL", where, colors_enabled); return;
        case Instruction::JUMP_FORWARD: instruction(chunk, "JUMP_FORWARD", where, colors_enabled); return;
        case Instruction::JUMP_BACKWARD: instruction(chunk, "JUMP_BACKWARD", where, colors_enabled); return;
        case Instruction::JUMP_IF_TRUE: instruction(chunk, "JUMP_IF_TRUE", where, colors_enabled); return;
        case Instruction::JUMP_IF_FALSE: instruction(chunk, "JUMP_IF_FALSE", where, colors_enabled); return;
        case Instruction::POP_JUMP_IF_EQUAL: instruction(chunk, "POP_JUMP_IF_EQUAL", where, colors_enabled); return;
        case Instruction::POP_JUMP_IF_FALSE: instruction(chunk, "POP_JUMP_IF_FALSE", where, colors_enabled); return;
        case Instruction::POP_JUMP_BACK_IF_TRUE:
            instruction(chunk, "POP_JUMP_BACK_IF_TRUE", where, colors_enabled);
            return;
        case Instruction::ASSIGN_LOCAL: instruction(chunk, "ASSIGN_LOCAL", where, colors_enabled); return;
        case Instruction::ACCESS_LOCAL: instruction(chunk, "ACCESS_LOCAL", where, colors_enabled); return;
        case Instruction::MAKE_REF_TO_LOCAL: instruction(chunk, "MAKE_REF_TO_LOCAL", where, colors_enabled); return;
        case Instruction::DEREF: instruction(chunk, "DEREF", where, colors_enabled); return;
        case Instruction::ASSIGN_GLOBAL: instruction(chunk, "ASSIGN_GLOBAL", where, colors_enabled); return;
        case Instruction::ACCESS_GLOBAL: instruction(chunk, "ACCESS_GLOBAL", where, colors_enabled); return;
        case Instruction::MAKE_REF_TO_GLOBAL: instruction(chunk, "MAKE_REF_TO_GLOBAL", where, colors_enabled); return;
        case Instruction::LOAD_FUNCTION_SAME_MODULE:
            instruction(chunk, "LOAD_FUNCTION_SAME_MODULE", where, colors_enabled);
            return;
        case Instruction::LOAD_FUNCTION_MODULE_INDEX:
            instruction(chunk, "LOAD_FUNCTION_MODULE_INDEX", where, colors_enabled);
            return;
        case Instruction::LOAD_FUNCTION_MODULE_PATH:
            instruction(chunk, "LOAD_FUNCTION_MODULE_PATH", where, colors_enabled);
            return;
        case Instruction::CALL_FUNCTION: instruction(chunk, "CALL_FUNCTION", where, colors_enabled); return;
        case Instruction::CALL_NATIVE: instruction(chunk, "CALL_NATIVE", where, colors_enabled); return;
        case Instruction::RETURN: instruction(chunk, "RETURN", where, colors_enabled); return;
        case Instruction::TRAP_RETURN: instruction(chunk, "TRAP_RETURN", where, colors_enabled); return;
        case Instruction::CONSTANT_STRING: instruction(chunk, "CONSTANT_STRING", where, colors_enabled); return;
        case Instruction::INDEX_STRING: instruction(chunk, "INDEX_STRING", where, colors_enabled); return;
        case Instruction::CHECK_STRING_INDEX: instruction(chunk, "CHECK_STRING_INDEX", where, colors_enabled); return;
        case Instruction::POP_STRING: instruction(chunk, "POP_STRING", where, colors_enabled); return;
        case Instruction::CONCATENATE: instruction(chunk, "CONCATENATE", where, colors_enabled); return;
        case Instruction::MAKE_LIST: instruction(chunk, "MAKE_LIST", where, colors_enabled); return;
        case Instruction::COPY_LIST: instruction(chunk, "COPY_LIST", where, colors_enabled); return;
        case Instruction::APPEND_LIST: instruction(chunk, "APPEND_LIST", where, colors_enabled); return;
        case Instruction::POP_FROM_LIST: instruction(chunk, "POP_FROM_LIST", where, colors_enabled); return;
        case Instruction::ASSIGN_LIST: instruction(chunk, "ASSIGN_LIST", where, colors_enabled); return;
        case Instruction::INDEX_LIST: instruction(chunk, "INDEX_LIST", where, colors_enabled); return;
        case Instruction::MAKE_REF_TO_INDEX: instruction(chunk, "MAKE_REF_TO_INDEX", where, colors_enabled); return;
        case Instruction::CHECK_LIST_INDEX: instruction(chunk, "CHECK_LIST_INDEX", where, colors_enabled); return;
        case Instruction::ACCESS_LOCAL_LIST: instruction(chunk, "ACCESS_LOCAL_LIST", where, colors_enabled); return;
        case Instruction::ACCESS_GLOBAL_LIST: instruction(chunk, "ACCESS_GLOBAL_LIST", where, colors_enabled); return;
        case Instruction::ASSIGN_LOCAL_LIST: instruction(chunk, "ASSIGN_LOCAL_LIST", where, colors_enabled); return;
        case Instruction::ASSIGN_GLOBAL_LIST: instruction(chunk, "ASSIGN_GLOBAL_LIST", where, colors_enabled); return;
        case Instruction::POP_LIST: instruction(chunk, "POP_LIST", where, colors_enabled); return;
        case Instruction::ACCESS_FROM_TOP: instruction(chunk, "ACCESS_FROM_TOP", where, colors_enabled); return;
        case Instruction::ASSIGN_FROM_TOP: instruction(chunk, "ASSIGN_FROM_TOP", where, colors_enabled); return;
        case Instruction::EQUAL_SL: instruction(chunk, "EQUAL_SL", where, colors_enabled); return;
        case Instruction::MOVE_LOCAL: instruction(chunk, "MOVE_LOCAL", where, colors_enabled); return;
        case Instruction::MOVE_GLOBAL: instruction(chunk, "MOVE_GLOBAL", where, colors_enabled); return;
        case Instruction::MOVE_INDEX: instruction(chunk, "MOVE_INDEX", where, colors_enabled); return;
        case Instruction::SWAP: instruction(chunk, "SWAP", where, colors_enabled); return;
    }
    unreachable();
}
