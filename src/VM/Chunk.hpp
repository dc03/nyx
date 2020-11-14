#pragma once

/* See LICENSE at project root for license details */

#ifndef CHUNK_HPP
#define CHUNK_HPP

#include "Instructions.hpp"
#include "Value.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct Chunk {
    using byte = unsigned char;
    std::vector<byte> bytes{};
    std::vector<Value> constants{};

    explicit Chunk() = default;
    void emit_byte(Chunk::byte value);
    void emit_bytes(Chunk::byte value_1, Chunk::byte value_2);
    void emit_constant(Value value);
    void emit_instruction(Instruction instruction);
};

#endif