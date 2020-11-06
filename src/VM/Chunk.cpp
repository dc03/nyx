/* See LICENSE at project root for license details */
#include "Chunk.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"
#include "Instructions.hpp"

#include <utility>

void Chunk::emit_byte(std::byte value) {
    bytes.push_back(value);
}

void Chunk::emit_bytes(std::byte value_1, std::byte value_2) {
    bytes.push_back(value_1);
    bytes.push_back(value_2);
}

void Chunk::emit_constant(Value value) {
    constexpr std::size_t const_short_max = (1 << 8) - 1;
    constexpr std::size_t const_long_max = (std::size_t{1} << 48) - 1;

    if (constants.size() <= const_short_max) {
        constants.emplace_back(std::move(value));
        emit_bytes(static_cast<std::byte>(Instruction::CONST_SHORT), static_cast<std::byte>(constants.size() - 1));
    } else if (constants.size() <= const_long_max) {
        constants.emplace_back(std::move(value));
        emit_bytes(static_cast<std::byte>(Instruction::CONST_LONG), static_cast<std::byte>(constants.size() - 1));
    } else {
        compile_error("Too many constants in chunk");
    }
}