#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef CHUNK_HPP
#define CHUNK_HPP

#include "Instructions.hpp"
#include "StringCacher.hpp"

#include <deque>
#include <string>
#include <utility>
#include <vector>

struct Value;

struct Chunk {
    static constexpr std::size_t const_short_max = (1 << 8) - 1;
    static constexpr std::size_t const_long_max = (std::size_t{1} << 24) - 1;

    using InstructionSizeType = std::uint32_t;

    std::vector<InstructionSizeType> bytes{};
    std::vector<Value> constants{};
    std::deque<HashedString> strings{};
    std::vector<std::pair<std::size_t, std::size_t>> line_numbers{};
    // Store line numbers of instructions using Run Length Encoding, first line number then instruction count for that
    // line

    explicit Chunk() = default;
    std::size_t add_constant(Value value);
    std::size_t add_string(std::string value);
    std::size_t emit_byte(Chunk::InstructionSizeType value);
    std::size_t emit_bytes(Chunk::InstructionSizeType value_1, Chunk::InstructionSizeType value_2);
    std::size_t emit_constant(Value value, std::size_t line_number);
    std::size_t emit_string(std::string value, std::size_t line_number);
    std::size_t emit_instruction(Instruction instruction, std::size_t line_number);

    std::size_t get_line_number(std::size_t insn_ptr);
};

#endif