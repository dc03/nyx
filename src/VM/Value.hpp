#pragma once

/* See LICENSE at project root for license details */

#ifndef VALUE_HPP
#define VALUE_HPP

#include <string>

struct Value {
    enum { INT, DOUBLE, STRING, BOOL, NULL_ } tag;
    union as {
        int integer;
        double real{};
        std::string string;
        bool boolean;
        std::nullptr_t null;

        as() : string{} {}
        ~as() {}

        explicit as(int value) : integer{value} {}
        explicit as(double value) : real{value} {}
        explicit as(const std::string &value) : string{value} {}
        explicit as(std::string &&value) : string{std::move(value)} {}
        explicit as(bool value) : boolean{value} {}
        explicit as(std::nullptr_t) : null{nullptr} {}
    } as;

    Value() : tag{NULL_} {}
    Value(Value &&other) noexcept;
    Value &operator=(Value &&other) noexcept;
    Value(const Value &other);
    Value &operator=(const Value &other);
    ~Value();

    explicit Value(int value);
    explicit Value(double value);
    explicit Value(std::string value);
    explicit Value(bool value);
    explicit Value(std::nullptr_t);

    // clang-format off
    [[nodiscard]] bool is_int()    const noexcept { return tag == Value::INT; }
    [[nodiscard]] bool is_double() const noexcept { return tag == Value::DOUBLE; }
    [[nodiscard]] bool is_string() const noexcept { return tag == Value::STRING; }
    [[nodiscard]] bool is_bool()   const noexcept { return tag == Value::BOOL; }
    [[nodiscard]] bool is_null()   const noexcept { return tag == Value::NULL_; }

    [[nodiscard]] int to_int()             const noexcept { return as.integer; }
    [[nodiscard]] double to_double()       const noexcept { return as.real; }
    [[nodiscard]] std::string to_string()  const noexcept { return as.string; }
    [[nodiscard]] bool to_bool()           const noexcept { return as.boolean; }
    [[nodiscard]] std::nullptr_t to_null() const noexcept { return as.null; }
    // clang-format on
};

#endif