#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef CHUNK_HPP
#define CHUNK_HPP

#include "Instructions.hpp"
#include "Value.hpp"

#include <string>
#include <utility>
#include <vector>

struct Chunk {
    static constexpr std::size_t const_short_max = (1 << 8) - 1;
    static constexpr std::size_t const_long_max = (std::size_t{1} << 24) - 1;

    using byte = unsigned char;
    std::vector<byte> bytes{};
    std::vector<Value> constants{};
    std::vector<std::pair<std::size_t, std::size_t>> line_numbers{};
    // Store line numbers of instructions using Run Length Encoding, first line number then instruction count for that
    // line

    explicit Chunk() = default;
    std::size_t add_constant(Value value);
    std::size_t emit_byte(Chunk::byte value);
    std::size_t emit_bytes(Chunk::byte value_1, Chunk::byte value_2);
    std::size_t emit_constant(Value value, std::size_t line_number);
    std::size_t emit_instruction(Instruction instruction, std::size_t line_number);
    std::size_t emit_integer(std::size_t integer);

    std::size_t get_line_number(std::size_t insn_number);
};

#endif