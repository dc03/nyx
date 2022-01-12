#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef DISASSEMBLER_HPP
#define DISASSEMBLER_HPP

#include "Backend/BackendContext.hpp"
#include "Backend/RuntimeModule.hpp"
#include "Chunk.hpp"
#include "Instructions.hpp"

#include <string_view>

void disassemble_ctx(BackendContext *ctx, bool colors_enabled);
void disassemble_module(RuntimeModule *module, bool colors_enabled);
void disassemble_chunk(Chunk &chunk, std::string_view module_name, std::string_view name, bool colors_enabled);
void disassemble_instruction(Chunk &chunk, Instruction instruction, std::size_t where, bool colors_enabled);

#endif