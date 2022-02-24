#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef CLI_CONFIG_PARSER_HPP
#define CLI_CONFIG_PARSER_HPP

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cxxopts {
class Options;
class ParseResult;
} // namespace cxxopts

// Options
#define COMPILE_OPTION      "Compile"
#define RUNTIME_OPTION      "Runtime"
#define SYNTAX_OPTION       "Syntax features"
#define OPTIMIZATION_OPTION "Optimization"

#define MAIN     "main"
#define CHECK    "check"
#define DUMP_AST "dump-ast"

#define IMPLICIT_FLOAT_INT    "implicit-float-int"
#define COMMA_OPERATOR        "comma-operator"
#define TERNARY_OPERATOR      "ternary-operator"
#define ASSIGNMENT_EXPRESSION "assignment-expr"

#define I_REALLY_KNOW_WHAT_IM_DOING_PLEASE_DONT_DESGUAR_THE_FOR_LOOP                                                   \
    "i-really-know-what-im-doing-please-dont-desugar-the-for-loop"
#define I_AM_THE_CODE_FORMATTER_DONT_COMPLAIN_ABOUT_FOR_LOOP "i-am-the-code-formatter-dont-complain-about-for-loop"

#define LANGUAGE_FEATURE_FLAG(name, description, default_)                                                             \
    {                                                                                                                  \
        name, {"warn", "error", "none"}, description " (supported: warn, error, none; default: " default_ ")",         \
            OptionType::QuantityTag::SINGLE_VALUE, OptionType::ValueTypeTag::STRING_VALUE, SYNTAX_OPTION               \
    }

#define CONSTANT_FOLDING "fold-constants"

#define OPTIMIZATION_FLAG(name, description, default_)                                                                 \
    {                                                                                                                  \
        name, {}, description " (supported: off, on; default: " default_ ")", OptionType::QuantityTag::SINGLE_VALUE,   \
            OptionType::ValueTypeTag::STRING_VALUE, OPTIMIZATION_OPTION                                                \
    }

#define NO_COLORIZE_OUTPUT "no-colorize-output"

#define DISASSEMBLE_CODE "disassemble-code"
#define TRACE_EXEC       "trace-exec"

class CLIConfig {
  public:
    enum ValueTag { STRING, STRING_SET, BOOL, BOOL_SET };
    using ValueType = std::variant<std::string, std::vector<std::string>, bool, std::vector<bool>>;
    using ConfigContainer = std::unordered_map<std::string, ValueType>;

    friend class CLIConfigParser;

  private:
    ConfigContainer config{};

  public:
    [[nodiscard]] bool contains(const std::string &key) const noexcept;

    template <typename T>
    [[nodiscard]] const T &get(const std::string &key) const {
        if (not contains(key)) {
            throw std::invalid_argument{"'" + key + "' does not exist"};
        } else if (not std::holds_alternative<T>(config.find(key)->second)) {
            throw std::invalid_argument{"'" + key + "' does not store requested type"};
        } else {
            return std::get<T>(config.find(key)->second);
        }
    }
};

class CLIConfigParser {
  public:
    struct OptionType {
        enum class QuantityTag { SINGLE_VALUE, MULTI_VALUE };

        enum class ValueTypeTag { BOOLEAN_VALUE, STRING_VALUE };

        std::string name{};
        std::vector<std::string> values{};
        std::string description{};

        QuantityTag quantity{};
        ValueTypeTag value{};

        std::string group{};
    };

    using Options = std::vector<OptionType>;

  private:
    int argc{};
    char **argv{};

    bool empty = false;
    bool help = false;
    std::string help_message{};

    CLIConfig compile_config{};
    CLIConfig runtime_config{};

    cxxopts::Options &options;

    enum EnabledTag {
        BASIC_ENABLED = 0b00001,
        LANG_FEAT_ENABLED = 0b00010,
        OPTIMIZATION_ENABLED = 0b00100,
        RUNTIME_ENABLED = 0b01000,
        SPECIAL_ENABLED = 0b10000
    };

    static const Options basic_options;
    static const Options language_feature_options;
    static const Options optimization_options;
    static const Options runtime_options;
    std::uint8_t enabled_options{};

    std::unordered_map<Options *, std::string_view> special_options{};

    void add_options(cxxopts::Options &options, const Options &values);
    void validate_args(cxxopts::ParseResult &result, const Options &values);
    void store_options(cxxopts::ParseResult &result, const Options &values, CLIConfig &into);

  public:
    CLIConfigParser(int argc, char **argv, cxxopts::Options &options);

    void add_basic_options() noexcept;
    void add_language_feature_options() noexcept;
    void add_optimization_options() noexcept;
    void add_runtime_options() noexcept;
    void add_special_options(Options *opts, std::string_view type);

    void parse_options();

    [[nodiscard]] const CLIConfig *get_compile_config() const noexcept;
    [[nodiscard]] const CLIConfig *get_runtime_config() const noexcept;

    template <typename T>
    void set_option(std::string_view type, const std::string &key, T &&value) {
        if (type == COMPILE_OPTION || type == SYNTAX_OPTION || type == OPTIMIZATION_OPTION) {
            compile_config.config[key] = std::forward<T>(value);
        } else if (type == RUNTIME_OPTION) {
            runtime_config.config[key] = std::forward<T>(value);
        } else {
            throw std::invalid_argument{"Unknown type for option; expected one of '" COMPILE_OPTION "', '" SYNTAX_OPTION
                                        "', '" OPTIMIZATION_OPTION "', '" RUNTIME_OPTION "'"};
        }
    }

    [[nodiscard]] bool is_empty() const noexcept;
    [[nodiscard]] bool is_help() const noexcept;
    [[nodiscard]] const std::string &get_help() const noexcept;
};

#endif