/* See LICENSE at project root for license details */
#include "VM.hpp"

void VM::push(const Value &value) {
    *(stack_top++) = value;
}

void VM::pop() {
    Value destroyed = std::move(*--stack_top);
}

Value &VM::top_from(std::size_t distance) const {
    return *(stack_top - distance);
}

Chunk::byte VM::read_byte() {
    return *(ip++);
}

void VM::run(RuntimeModule &main_module) {
    ip = &main_module.top_level_code.bytes[0];
    chunk = &main_module.top_level_code;

#define is(x) static_cast<unsigned char>(x)

    for (;;) {
        switch (read_byte()) {
            case is(Instruction::CONST_SHORT): push(chunk->constants[read_byte()]); break;
            case is(Instruction::ADD):
                if (top_from(2).is_int() && top_from(1).is_int()) {
                    Value added{top_from(2).to_int() + top_from(1).to_int()};
                    pop();
                    pop();
                    push(added);
                }
                break;
            case is(Instruction::POP): pop(); break;
            case is(Instruction::HALT): return;
        }
    }

#undef is
}