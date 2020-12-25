#pragma once

/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef DISASSEMBLER_HPP
#define DISASSEMBLER_HPP

#include "../VM/Chunk.hpp"
#include "../VM/Instructions.hpp"

#include <iostream>
#include <string_view>

void disassemble(Chunk &chunk, std::string_view name);
std::size_t disassemble_instruction(Chunk &chunk, Instruction instruction, std::size_t byte, std::size_t insn_count);

#endif