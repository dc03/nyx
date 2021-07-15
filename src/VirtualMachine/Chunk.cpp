/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Chunk.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"
#include "Value.hpp"

#include <limits>
#include <utility>

std::size_t Chunk::add_constant(Value value) {
    constants.emplace_back(value);
    return constants.size() - 1;
}

std::size_t Chunk::add_string(std::string value) {
    strings.emplace_back(std::move(value));
    constants.emplace_back(Value{&strings.back()});
    return constants.size() - 1;
}

std::size_t Chunk::emit_byte(Chunk::InstructionSizeType value) {
    bytes.back() |= value;
    if (line_numbers.empty()) {
        line_numbers.emplace_back(1, 1);
    } else {
        line_numbers.back().second += 1;
    }
    return bytes.size() - 1;
}

std::size_t Chunk::emit_bytes(Chunk::InstructionSizeType value_1, Chunk::InstructionSizeType value_2) {
    bytes.back() |= value_1 << 8;
    bytes.back() |= value_2 << 16;
    if (line_numbers.empty()) {
        line_numbers.emplace_back(1, 2);
    } else {
        line_numbers.back().second += 2;
    }
    return bytes.size() - 2;
}

std::size_t Chunk::emit_constant(Value value, std::size_t line_number) {
    if (constants.size() < const_long_max) {
        std::size_t constant = add_constant(value);
        emit_instruction(Instruction::CONSTANT, line_number);
        emit_bytes((constant >> 16) & 0xff, (constant >> 8) & 0xff);
        emit_byte(constant & 0xff);
        return bytes.size() - 4;
    } else {
        compile_error("Too many constants in chunk");
        return 0;
    }
}

std::size_t Chunk::emit_string(std::string value, std::size_t line_number) {
    if (constants.size() < const_long_max) {
        std::size_t constant = add_string(std::move(value));
        emit_instruction(Instruction::CONSTANT_STRING, line_number);
        emit_bytes((constant >> 16) & 0xff, (constant >> 8) & 0xff);
        emit_byte(constant & 0xff);
        return bytes.size() - 4;
    } else {
        compile_error("Too many constants in chunk");
        return 0;
    }
}

std::size_t Chunk::emit_instruction(Instruction instruction, std::size_t line_number) {
    bytes.push_back(static_cast<InstructionSizeType>(instruction) << 24);
    bytes.back() &= 0xff00'0000;
    if (line_numbers.empty() || line_numbers.back().first != line_number) {
        line_numbers.emplace_back(line_number, 1);
    } else {
        line_numbers.back().second += 1;
    }
    return bytes.size() - 1;
}

std::size_t Chunk::get_line_number(std::size_t insn_ptr) {
    std::size_t i = 0;
    long long signed_insn_number = insn_ptr;
    while (signed_insn_number >= 0 && i < line_numbers.size()) {
        signed_insn_number -= static_cast<long long>(line_numbers[i].second);
        i++;
    }
    return line_numbers[i - 1].first;
}