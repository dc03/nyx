/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "CLIConfigParser.hpp"

#include <cstdlib>
#include <cxxopts.hpp>
#include <numeric>

bool CLIConfig::contains(const std::string &key) const noexcept {
    return config.find(key) != config.end();
}

// clang-format off
const CLIConfigParser::Options CLIConfigParser::compile_options{
    {"main", {}, "The module from which to start execution",
        OptionType::QuantityTag::SINGLE_VALUE,
        OptionType::ValueTypeTag::STRING_VALUE},
    {"check", {}, "Do not run the code, only parse and type check it",
        OptionType::QuantityTag::SINGLE_VALUE,
        OptionType::ValueTypeTag::BOOLEAN_VALUE},
    {"dump-ast", {}, "Dump the contents of the AST after parsing and typechecking",
        OptionType::QuantityTag::SINGLE_VALUE,
        OptionType::ValueTypeTag::BOOLEAN_VALUE},
    {"implicit-float-int", {"warn", "error", "none"}, "Warning/error about implicit conversion between float and int (supported: warn, error, none; default: warn)",
        OptionType::QuantityTag::SINGLE_VALUE,
        OptionType::ValueTypeTag::STRING_VALUE}};

const CLIConfigParser::Options CLIConfigParser::runtime_options{
    {"disassemble-code", {}, "Disassemble the byte code produced for the VM",
        OptionType::QuantityTag::SINGLE_VALUE,
        OptionType::ValueTypeTag::BOOLEAN_VALUE},
    {"trace-exec", {"stack", "frame", "module", "insn", "module_init"}, "Print information during execution (supported: stack, frame, module, insn, module_init)",
        OptionType::QuantityTag::MULTI_VALUE,
        OptionType::ValueTypeTag::STRING_VALUE},
};
// clang-format on

void CLIConfigParser::add_options(cxxopts::Options &options, const Options &values, const std::string &group) {
    for (auto &option : values) {
        if (option.quantity == OptionType::QuantityTag::SINGLE_VALUE) {
            options.add_options(group)(option.name, option.description,
                option.value == OptionType::ValueTypeTag::BOOLEAN_VALUE ? cxxopts::value<bool>()->default_value("false")
                                                                        : cxxopts::value<std::string>());
        } else if (option.quantity == OptionType::QuantityTag::MULTI_VALUE) {
            options.add_options(group)(option.name, option.description,
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

    add_options(options, compile_options, "Compile");
    add_options(options, runtime_options, "Runtime");

    options.add_options()("no-colorize-output", "Do not colorize output");
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

            if (result.count("no-colorize-output")) {
                compile_config.config["no-colorize-output"] = true;
                runtime_config.config["no-colorize-output"] = true;
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