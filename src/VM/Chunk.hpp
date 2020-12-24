#pragma once

/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef CHUNK_HPP
#define CHUNK_HPP

#include "Instructions.hpp"
#include "Value.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct Chunk {
    static constexpr std::size_t const_short_max = (1 << 8) - 1;
    static constexpr std::size_t const_long_max = (std::size_t{1} << 24) - 1;

    using byte = unsigned char;
    std::vector<byte> bytes{};
    std::vector<Value> constants{};

    explicit Chunk() = default;
    std::size_t add_constant(Value value);
    std::size_t emit_byte(Chunk::byte value);
    std::size_t emit_bytes(Chunk::byte value_1, Chunk::byte value_2);
    std::size_t emit_constant(Value value);
    std::size_t emit_instruction(Instruction instruction);
};

#endif