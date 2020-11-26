/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Chunk.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"

#include <utility>

void Chunk::emit_byte(Chunk::byte value) {
    bytes.push_back(value);
}

void Chunk::emit_bytes(Chunk::byte value_1, Chunk::byte value_2) {
    bytes.push_back(value_1);
    bytes.push_back(value_2);
}

void Chunk::emit_constant(Value value) {
    constexpr std::size_t const_short_max = (1 << 8) - 1;
    constexpr std::size_t const_long_max = (std::size_t{1} << 48) - 1;

    if (constants.size() <= const_short_max) {
        constants.emplace_back(std::move(value));
        emit_bytes(static_cast<unsigned char>(Instruction::CONST_SHORT), constants.size() - 1);
    } else if (constants.size() <= const_long_max) {
        constants.emplace_back(std::move(value));
        emit_bytes(static_cast<unsigned char>(Instruction::CONST_LONG), constants.size() - 1);
    } else {
        compile_error("Too many constants in chunk");
    }
}

void Chunk::emit_instruction(Instruction instruction) {
    bytes.push_back(static_cast<unsigned char>(instruction));
}