/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "nyx/AST/ASTPrinter.hpp"
#include "nyx/CLIConfigParser.hpp"
#include "nyx/Frontend/FrontendManager.hpp"
#include "nyx/NyxFormatter.hpp"

#include <cstdlib>
#include <cxxopts.hpp>
#include <iostream>

void run(const char *const main_module, const CLIConfig *compile_config) {
    FrontendContext compile_ctx{};
    compile_ctx.set_config(compile_config);

    FrontendManager compile_manager{&compile_ctx, main_module, true, 0};

    compile_manager.parse_module();
    compile_manager.check_module();

    if (compile_config->contains(DUMP_AST)) {
        ASTPrinter printer{};
        for (auto &[module, depth] : compile_ctx.parsed_modules) {
            std::cout << "-<=== Module " << module.name << " ===>-\n\n";
            printer.print_stmts(module.statements);
        }
        std::cout << "-<=== Main Module ===>-\n";
        std::cout << "-<=== Module " << compile_manager.get_module().name << " ===>-\n\n";
        printer.print_stmts(compile_manager.get_module().statements);
    }

    NyxFormatter formatter{std::cout, &compile_ctx};
    formatter.format(compile_manager.get_module());
}

bool is_integer(const std::string &str) {
    if (str.empty() || ((not std::isdigit(str[0])) && (str[0] != '-') && (str[0] != '+'))) {
        return false;
    }

    char *p;
    std::strtol(str.c_str(), &p, 10);
    return *p == 0;
}

int main(int argc, char *argv[]) {
    try {
        // clang-format off
        CLIConfigParser::Options formatter_options{
            {USE_TABS,{"yes", "no"}, "Use tabs for formatting (supported: yes, no; default: no)",
                CLIConfigParser::OptionType::QuantityTag::SINGLE_VALUE,
                CLIConfigParser::OptionType::ValueTypeTag::STRING_VALUE, "Formatter"},
            {TAB_SIZE, {}, "The tab size to use for indentation (only applicable when indenting with spaces; default: 4)",
                CLIConfigParser::OptionType::QuantityTag::SINGLE_VALUE,
                CLIConfigParser::OptionType::ValueTypeTag::STRING_VALUE, "Formatter"},
            {COLLAPSE_SINGLE_LINE_BLOCK, {"yes", "no"}, "Collapse blocks containing a single statement into a single line (supported: yes, no; default: no)",
                CLIConfigParser::OptionType::QuantityTag::SINGLE_VALUE,
                CLIConfigParser::OptionType::ValueTypeTag::STRING_VALUE, "Formatter"},
            {BRACE_NEXT_LINE, {"all", "class", "for", "function", "if", "switch", "while"},
                "Put braces after the statement on the next line (supported: all, class, for, function, if, switch, while)",
                CLIConfigParser::OptionType::QuantityTag::MULTI_VALUE,
                CLIConfigParser::OptionType::ValueTypeTag::STRING_VALUE, "Formatter"}};
        // clang-format on

        cxxopts::Options options{argv[0], "A small and simple interpreted language"};
        CLIConfigParser parser{argc, argv, options};
        parser.add_basic_options();
        parser.add_language_feature_options();
        parser.add_special_options(&formatter_options, COMPILE_OPTION);
        parser.parse_options();

        parser.set_option<std::string>(COMPILE_OPTION, CONSTANT_FOLDING, "off");
        parser.set_option<std::string>(
            COMPILE_OPTION, I_REALLY_KNOW_WHAT_IM_DOING_PLEASE_DONT_DESGUAR_THE_FOR_LOOP, "on");
        parser.set_option<std::string>(COMPILE_OPTION, I_AM_THE_CODE_FORMATTER_DONT_COMPLAIN_ABOUT_FOR_LOOP, "on");
        const CLIConfig *compile_config = parser.get_compile_config();

        if (compile_config->contains(USE_TABS) && compile_config->get<std::string>(USE_TABS) == "yes" &&
            compile_config->contains(TAB_SIZE)) {
            std::cerr << "Error: option '" USE_TABS "' and '" TAB_SIZE "' cannot be used together\n";
            return 0;
        } else if (compile_config->contains(TAB_SIZE)) {
            const auto &value = compile_config->get<std::string>(TAB_SIZE);
            if (not is_integer(value)) {
                std::cerr << "Error: option '" TAB_SIZE << "' accepts only integral values\n";
                return 0;
            }
        }

        if (parser.is_empty() || parser.is_help()) {
            std::cout << parser.get_help() << '\n';
            return 0;
        } else if (compile_config->contains(MAIN)) {
            run(compile_config->get<std::string>(MAIN).c_str(), compile_config);
        }
    } catch (const std::invalid_argument &e) { std::cout << e.what() << '\n'; }

    return 0;
}