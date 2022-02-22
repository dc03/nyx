/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "nyx/CLIConfigParser.hpp"

#include <cstdlib>
#include <cxxopts.hpp>
#include <numeric>

bool CLIConfig::contains(const std::string &key) const noexcept {
    return config.find(key) != config.end();
}

// clang-format off
const CLIConfigParser::Options CLIConfigParser::compile_options{
    {MAIN, {}, "The module from which to start execution",
        OptionType::QuantityTag::SINGLE_VALUE,
        OptionType::ValueTypeTag::STRING_VALUE, "Compile"},
    {CHECK, {}, "Do not run the code, only parse and type check it",
        OptionType::QuantityTag::SINGLE_VALUE,
        OptionType::ValueTypeTag::BOOLEAN_VALUE, "Compile"},
    {DUMP_AST, {}, "Dump the contents of the AST after parsing and typechecking",
        OptionType::QuantityTag::SINGLE_VALUE,
        OptionType::ValueTypeTag::BOOLEAN_VALUE, "Compile"},
    LANGUAGE_FEATURE_FLAG(IMPLICIT_FLOAT_INT, "Warning/error about implicit conversion between float and int", "warn"),
    LANGUAGE_FEATURE_FLAG(COMMA_OPERATOR, "Warning/error about the usage of comma operator", "error"),
    LANGUAGE_FEATURE_FLAG(TERNARY_OPERATOR, "Warning/error about the usage of ternary operator", "error"),
    LANGUAGE_FEATURE_FLAG(ASSIGNMENT_EXPRESSION, "Warning/error for when variable assignments not used as standalone statements ", "error"),
    OPTIMIZATION_FLAG(CONSTANT_FOLDING, "Simplify expressions containing constant values (such as '5 + 6') into their computed values ('11')", "on"),
};

const CLIConfigParser::Options CLIConfigParser::runtime_options{
    {DISASSEMBLE_CODE, {}, "Disassemble the byte code produced for the VM",
        OptionType::QuantityTag::SINGLE_VALUE,
        OptionType::ValueTypeTag::BOOLEAN_VALUE, "Runtime"},
    {TRACE_EXEC, {"stack", "frame", "module", "insn", "module_init"}, "Print information during execution (supported: stack, frame, module, insn, module_init)",
        OptionType::QuantityTag::MULTI_VALUE,
        OptionType::ValueTypeTag::STRING_VALUE, "Runtime"},
};
// clang-format on

void CLIConfigParser::add_options(cxxopts::Options &options, const Options &values) {
    for (auto &option : values) {
        if (option.quantity == OptionType::QuantityTag::SINGLE_VALUE) {
            options.add_options(option.group)(option.name, option.description,
                option.value == OptionType::ValueTypeTag::BOOLEAN_VALUE ? cxxopts::value<bool>()->default_value("false")
                                                                        : cxxopts::value<std::string>());
        } else if (option.quantity == OptionType::QuantityTag::MULTI_VALUE) {
            options.add_options(option.group)(option.name, option.description,
                option.value == OptionType::ValueTypeTag::BOOLEAN_VALUE ? cxxopts::value<std::vector<bool>>()
                                                                        : cxxopts::value<std::vector<std::string>>());
        }
    }
}

void CLIConfigParser::validate_args(cxxopts::ParseResult &result, const Options &values) {
    for (auto &option : values) {
        if (not option.values.empty() && option.quantity == OptionType::QuantityTag::MULTI_VALUE &&
            result.count(option.name) > 0) {
            std::vector<std::string> args = result[option.name].as<std::vector<std::string>>();
            for (std::string &arg : args) {
                if (std::find(option.values.begin(), option.values.end(), arg) == option.values.end()) {
                    throw std::invalid_argument{
                        "Error: incorrect argument '" + arg + "' to option '" + option.name +
                        "', permitted values are: '" +
                        std::accumulate(option.values.begin() + 1, option.values.end(), option.values[0],
                            [](const std::string &one, const std::string &two) { return one + "," + two; }) +
                        "'"};
                }
            }
        }
    }
}

void CLIConfigParser::store_options(cxxopts::ParseResult &result, const Options &values, CLIConfig &into) {
    for (auto &option : values) {
        if (result.count(option.name) > 0) {
            if (option.quantity == OptionType::QuantityTag::SINGLE_VALUE &&
                option.value == OptionType::ValueTypeTag::BOOLEAN_VALUE) {
                into.config[option.name] = result[option.name].as<bool>();
            } else if (option.quantity == OptionType::QuantityTag::SINGLE_VALUE &&
                       option.value == OptionType::ValueTypeTag::STRING_VALUE) {
                into.config[option.name] = result[option.name].as<std::string>();
            } else if (option.quantity == OptionType::QuantityTag::MULTI_VALUE &&
                       option.value == OptionType::ValueTypeTag::STRING_VALUE) {
                into.config[option.name] = result[option.name].as<std::vector<std::string>>();
            }
        }
    }
}

CLIConfigParser::CLIConfigParser(int argc, char **argv) : argc{argc}, argv{argv} {
    cxxopts::Options options{argv[0], "A small and simple interpreted language"};

    add_options(options, compile_options);
    add_options(options, runtime_options);

    options.add_options()(NO_COLORIZE_OUTPUT, "Do not colorize output");
    options.add_options()("h,help", "Print usage");

    try {
        cxxopts::ParseResult result = options.parse(argc, argv);

        if (result.arguments().empty()) {
            empty = true;
            help_message = options.help();
        } else if (result.count("help")) {
            help = true;
            help_message = options.help();
        } else {
            validate_args(result, compile_options);
            validate_args(result, runtime_options);

            store_options(result, compile_options, compile_config);
            store_options(result, runtime_options, runtime_config);

            if (result.count(NO_COLORIZE_OUTPUT)) {
                compile_config.config[NO_COLORIZE_OUTPUT] = true;
                runtime_config.config[NO_COLORIZE_OUTPUT] = true;
            }
        }
    } catch (const cxxopts::OptionException &e) {
        std::cout << e.what() << '\n';
        std::exit(0);
    } catch (const std::invalid_argument &e) {
        std::cout << e.what() << '\n';
        std::exit(0);
    }
}

const CLIConfig *CLIConfigParser::get_compile_config() const noexcept {
    return &compile_config;
}

const CLIConfig *CLIConfigParser::get_runtime_config() const noexcept {
    return &runtime_config;
}

bool CLIConfigParser::is_empty() const noexcept {
    return empty;
}

bool CLIConfigParser::is_help() const noexcept {
    return help;
}

const std::string &CLIConfigParser::get_help() const noexcept {
    return help_message;
}