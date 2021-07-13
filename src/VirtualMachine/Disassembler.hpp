#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef DISASSEMBLER_HPP
#define DISASSEMBLER_HPP

#include "Chunk.hpp"
#include "Instructions.hpp"

#include <string_view>

void disassemble(Chunk &chunk, std::string_view name);
std::size_t disassemble_instruction(Chunk &chunk, Instruction instruction, std::size_t byte);

#endif