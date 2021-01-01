#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VALUE_HPP
#define VALUE_HPP

#include <cstring>
#include <string>

struct RuntimeFunction;

struct Value {
    enum { INT, DOUBLE, STRING, BOOL, NULL_, PRIMITIVE_REF, FUNCTION } tag;
    union as {
        int integer;
        double real;
        char *string;
        bool boolean;
        std::nullptr_t null;
        Value *reference;
        RuntimeFunction *function;

        as() : null{nullptr} {}
        ~as() {}

        explicit as(int value) : integer{value} {}
        explicit as(double value) : real{value} {}
        explicit as(const char *value) {
            std::size_t len = std::strlen(value);
            string = new char[len + 1];
            std::memcpy(string, value, len);
            string[len] = '\0';
        }
        explicit as(bool value) : boolean{value} {}
        explicit as(std::nullptr_t) : null{nullptr} {}
        explicit as(Value *referred) : reference{referred} {}
        explicit as(RuntimeFunction *function) : function{function} {}
    } as;

    Value() : tag{NULL_} {}
    Value(Value &&other) noexcept;
    Value &operator=(Value &&other) noexcept;
    Value(const Value &other);
    Value &operator=(const Value &other);
    ~Value();

    explicit Value(int value);
    explicit Value(double value);
    explicit Value(const char *value);
    explicit Value(bool value);
    explicit Value(std::nullptr_t);
    explicit Value(Value *referred);
    explicit Value(RuntimeFunction *function);

    // clang-format off
    [[nodiscard]] bool is_int()    const noexcept { return tag == Value::INT; }
    [[nodiscard]] bool is_double() const noexcept { return tag == Value::DOUBLE; }
    [[nodiscard]] bool is_string() const noexcept { return tag == Value::STRING; }
    [[nodiscard]] bool is_bool()   const noexcept { return tag == Value::BOOL; }
    [[nodiscard]] bool is_null()   const noexcept { return tag == Value::NULL_; }
    [[nodiscard]] bool is_numeric() const noexcept { return tag == Value::INT || tag == Value::DOUBLE; }
    [[nodiscard]] bool is_ref()    const noexcept { return tag == Value::PRIMITIVE_REF; }
    [[nodiscard]] bool is_function() const noexcept { return tag == Value::FUNCTION; }

    [[nodiscard]] int to_int()             const noexcept { return as.integer; }
    [[nodiscard]] double to_double()       const noexcept { return as.real; }
    [[nodiscard]] char *to_string()  const noexcept { return as.string; }
    [[nodiscard]] bool to_bool()           const noexcept { return as.boolean; }
    [[nodiscard]] std::nullptr_t to_null() const noexcept { return as.null; }
    [[nodiscard]] double to_numeric()      const noexcept { return is_int() ? as.integer : as.real; }
    [[nodiscard]] Value *to_referred()     const noexcept { return as.reference; }
    [[nodiscard]] RuntimeFunction *to_function() const noexcept { return as.function; }
    // clang-format on

    [[nodiscard]] bool operator==(const Value &other) const noexcept;
    [[nodiscard]] std::string repr() const noexcept;
};

#endif