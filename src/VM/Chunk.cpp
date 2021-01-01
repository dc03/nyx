/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Chunk.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"

#include <limits>
#include <utility>

std::size_t Chunk::add_constant(Value value) {
    constants.emplace_back(std::move(value));
    return constants.size() - 1;
}

std::size_t Chunk::emit_byte(Chunk::byte value) {
    bytes.push_back(value);
    return bytes.size() - 1;
}

std::size_t Chunk::emit_bytes(Chunk::byte value_1, Chunk::byte value_2) {
    bytes.push_back(value_1);
    bytes.push_back(value_2);
    return bytes.size() - 2;
}

std::size_t Chunk::emit_constant(Value value, std::size_t line_number) {
    if (constants.size() < const_short_max) {
        emit_instruction(Instruction::CONST_SHORT, line_number);
        emit_byte(add_constant(std::move(value)));
        return bytes.size() - 2;
    } else if (constants.size() < const_long_max) {
        std::size_t constant = add_constant(std::move(value));
        emit_instruction(Instruction::CONST_LONG, line_number);
        emit_bytes((constant >> 16) & 0xff, (constant >> 8) & 0xff);
        emit_byte(constant & 0xff);
        return bytes.size() - 4;
    } else {
        compile_error("Too many constants in chunk");
        return 0;
    }
}

std::size_t Chunk::emit_instruction(Instruction instruction, std::size_t line_number) {
    bytes.push_back(static_cast<unsigned char>(instruction));
    if (line_numbers.empty() || line_numbers.back().first != line_number) {
        line_numbers.emplace_back(line_number, 1);
    } else {
        line_numbers.back().second += 1;
    }
    return bytes.size() - 1;
}

std::size_t Chunk::emit_integer(std::size_t integer) {
    if (integer < Chunk::const_short_max) {
        emit_byte(integer & 0xff);
        return bytes.size() - 1;
    } else if (integer < Chunk::const_long_max) {
        emit_bytes((integer >> 16) & 0xff, (integer >> 8) & 0xff);
        emit_byte(integer & 0xff);
        return bytes.size() - 3;
    } else {
        compile_error("Integer is too large to fit in upto 3 bytes");
        return std::numeric_limits<std::size_t>::max();
    }
}

std::size_t Chunk::get_line_number(std::size_t insn_number) {
    std::size_t i = 0;
    long long signed_insn_number = insn_number;
    while (signed_insn_number > 0 && i < line_numbers.size()) {
        signed_insn_number -= static_cast<long long>(line_numbers[i].second);
        i++;
    }
    return line_numbers[i - 1].first;
}