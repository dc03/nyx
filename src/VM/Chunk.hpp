#pragma once

/* See LICENSE at project root for license details */

#ifndef CHUNK_HPP
#define CHUNK_HPP

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

struct Value {
    std::variant<int, double, std::string, bool, std::nullptr_t> value{};
};

struct Chunk {
    std::vector<std::byte> bytes{};
    std::byte *ip{};
    std::vector<Value> constants{};

    explicit Chunk() = default;
    void emit_byte(std::byte value);
    void emit_bytes(std::byte value_1, std::byte value_2);
    void emit_constant(Value value);
};

#endif