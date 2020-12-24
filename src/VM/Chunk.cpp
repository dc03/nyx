/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Chunk.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"

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

std::size_t Chunk::emit_constant(Value value) {
    if (constants.size() < const_short_max) {
        return emit_bytes(static_cast<unsigned char>(Instruction::CONST_SHORT), add_constant(std::move(value)));
    } else if (constants.size() < const_long_max) {
        std::size_t constant = add_constant(std::move(value));
        std::size_t insn = emit_bytes(static_cast<unsigned char>(Instruction::CONST_LONG), (constant >> 16) & 0xff);
        emit_bytes((constant >> 8) & 0xff, constant & 0xff);
        return insn;
    } else {
        compile_error("Too many constants in chunk");
        return 0;
    }
}

std::size_t Chunk::emit_instruction(Instruction instruction) {
    bytes.push_back(static_cast<unsigned char>(instruction));
    return bytes.size() - 1;
}