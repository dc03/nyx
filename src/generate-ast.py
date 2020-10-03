# See LICENSE at project root for license details
from typing import *


def make_header(file, guard: str) -> None:
    file.write('#pragma once\n\n')
    file.write('/* See LICENSE at project root for license details */\n\n')
    file.write('#ifndef ' + guard + '\n')
    file.write('#  define ' + guard + '\n\n')
    return None


def end_file(file) -> None:
    file.write('#endif\n')
    return None


def forward_declare(file, names: List[str]) -> None:
    for name in names:
        file.write('template <typename T>\n')
        file.write('struct ' + name + ';\n')


def declare_alias(file, alias: str, name: str) -> None:
    file.write('template <typename T>\n')
    file.write('using ' + alias + ' = std::unique_ptr<' + name + '<T>>;\n')
    return None


def tab(file, n: int):
    file.write('    ' * n)
    return file


def declare_visitor(file, visitor: str, exprs: List[str], stmts: List[str], expr_base: str, stmt_base: str,
                    types: List[str], type_base: str) -> None:
    file.write('template <typename T>\n')
    file.write('struct ' + visitor + ' {\n')
    for expr in exprs:
        tab(file, 1).write(
            'virtual T visit(' + expr + '<T>& ' + expr_base.lower() + ') = 0;\n')

    file.write('\n')

    for stmt in stmts:
        tab(file, 1).write(
            'virtual T visit(' + stmt + '<T>& ' + stmt_base.lower() + ') = 0;\n')

    file.write('\n')

    for type in types:
        tab(file, 1).write(
            'virtual T visit(' + type + '<T>& ' + stmt_base.lower() + ') = 0;\n')

    file.write('};\n\n')
    return None


def declare_base(file, base_name: str) -> None:
    file.write('template <typename T>\n')
    file.write('struct ' + base_name + ' {\n')
    tab(file, 1).write('virtual std::string_view to_string() = 0;\n')
    tab(file, 1).write('virtual T accept(Visitor<T>& visitor) = 0;\n')
    tab(file, 1).write('virtual ~' + base_name + '() = default;\n')
    file.write('};\n\n')
    return None


def declare_derived(file, base_name: str, derived_name: str, ctor_args: str, members: str) -> None:
    file.write('template <typename T>\n')
    file.write('struct ' + derived_name +
               ' final: public ' + base_name + '<T> {\n')

    if members != '':
        for member in members.split(', '):
            tab(file, 1).write(member.strip() + ';\n')
        file.write('\n')
        # Class members

    tab(file, 1).write('std::string_view to_string() override final {\n')
    tab(file, 2).write('return "' + derived_name + '";\n')
    tab(file, 1).write('}\n\n')
    # to_string() method

    tab(file, 1).write(derived_name + '(' + members + ')' + (':\n' if ctor_args != '' else '\n'))
    tab(file, 2).write(ctor_args + ' {}\n')
    file.write('\n')
    # Class constructor

    tab(file, 1).write('T accept(Visitor<T>& visitor) override final {\n')
    tab(file, 2).write('return visitor.visit(*this);\n')
    tab(file, 1).write('}\n')
    # Accept method
    file.write('};\n\n')
    return None


expr_count: int = 0
stmt_count: int = 0


def decl_expr(ctor_args: str, members: str) -> None:
    global expr_count
    declare_derived(file, 'Expr', Exprs[expr_count], ctor_args, members)
    expr_count += 1
    return None


def decl_stmt(ctor_args: str, members: str) -> None:
    global stmt_count
    declare_derived(file, 'Stmt', Stmts[stmt_count], ctor_args, members)
    stmt_count += 1
    return None


if __name__ == '__main__':
    with open('AST.hpp', 'wt') as file:
        make_header(file, 'AST_HPP')
        file.write('#include <memory>\n')
        file.write('#include <optional>\n')
        file.write('#include <string_view>\n')
        file.write('#include <utility>\n')
        file.write('#include <vector>\n')
        file.write('\n#include "Token.hpp"\n\n')
        forward_declare(file, ['Expr', 'Stmt', 'Type'])
        file.write('\n')
        declare_alias(file, 'expr_node_t', 'Expr')
        declare_alias(file, 'stmt_node_t', 'Stmt')
        declare_alias(file, 'type_node_t', 'Type')
        # Base class and alias declarations complete

        Exprs: List[str] = ['Assign', 'Binary', 'Call', 'Comma', 'Get', 'Grouping', 'Index', 'Literal',
                            'Logical', 'Set', 'Super', 'Ternary', 'This', 'Unary', 'Variable']
        Stmts: List[str] = ['Block', 'Break', 'Class', 'Continue', 'Expression', 'Function',
                            'If', 'Import', 'Return', 'Switch', 'Type', 'Var', 'While']
        Types: List[str] = ['Primitive', 'UserDefined', 'List']
        Exprs: List[str] = [x + 'Expr' for x in Exprs]
        Stmts: List[str] = [x + 'Stmt' for x in Stmts]
        Types: List[str] = [x + 'Type' for x in Types]
        file.write('\n// Expression nodes\n\n')
        forward_declare(file, Exprs)
        file.write('\n// Statement nodes\n\n')
        forward_declare(file, Stmts)
        file.write('\n// Type nodes\n\n')
        forward_declare(file, Types)
        file.write('\n')
        # Forward declarations complete

        declare_visitor(file, 'Visitor', Exprs, Stmts, 'Expr', 'Stmt', Types, 'Type')
        # Visitor declaration complete

        declare_base(file, 'Expr')
        declare_base(file, 'Stmt')

        file.write('enum class TypeType {\n')
        tab(file, 1).write('INT,\n')
        tab(file, 1).write('FLOAT,\n')
        tab(file, 1).write('STRING,\n')
        tab(file, 1).write('CLASS,\n')
        tab(file, 1).write('LIST\n')
        file.write('};\n\n')

        file.write('template <typename T>\n')
        file.write('struct Type {\n')
        tab(file, 1).write('TypeType type;\n')
        tab(file, 1).write('bool is_const;\n')
        tab(file, 1).write('bool is_ref;\n\n')
        tab(file, 1).write('virtual T accept(Visitor<T>& visitor) = 0;\n')
        tab(file, 1).write('virtual ~Type() = default;\n')
        file.write('};\n\n')
        # Type base node definition

        # NOTE: Always make sure that these are in the same order as the lists

        file.write('// Type node definitions\n\n')

        declare_derived(file, 'Type', Types[0], '', '')

        declare_derived(file, 'Type', Types[1], 'name{name}', 'Token name')

        declare_derived(file, 'Type', Types[2], 'contained{std::move(contained)}',
                        'type_node_t<T> contained')

        file.write('// End of type node definitions\n\n')

        file.write('// Expression node definitions\n\n')

        decl_expr('target{target}, value{std::move(value)}',
                  'Token target, expr_node_t<T> value')

        decl_expr('left{std::move(left)}, oper{oper}, right{std::move(right)}',
                  'expr_node_t<T> left, Token oper, expr_node_t<T> right')

        decl_expr('function{std::move(function)}, paren{paren}, args{std::move(args)}',
                  'expr_node_t<T> function, Token paren, std::vector<expr_node_t<T>> args')

        decl_expr('exprs{std::move(exprs)}',
                  'std::vector<expr_node_t<T>> exprs')

        decl_expr('object{std::move(object)}, name{name}',
                  'expr_node_t<T> object, Token name')

        decl_expr('expr{std::move(expr)}',
                  'expr_node_t<T> expr')

        decl_expr('object{std::move(object)}, oper{oper}, index{std::move(index)}',
                  'expr_node_t<T> object, Token oper, expr_node_t<T> index')

        decl_expr('value{std::move(value)}',
                  'T value')

        decl_expr('left{std::move(left)}, oper{oper}, right{std::move(right)}',
                  'expr_node_t<T> left, Token oper, expr_node_t<T> right')

        decl_expr('object{std::move(object)}, name{name}, value{std::move(value)}',
                  'expr_node_t<T> object, Token name, expr_node_t<T> value')

        decl_expr('keyword{keyword}, name{name}',
                  'Token keyword, Token name')

        decl_expr('left{std::move(left)}, question{question}, middle{std::move(middle)}, right{std::move(right)}',
                  'expr_node_t<T> left, Token question, expr_node_t<T> middle, expr_node_t<T> right')

        decl_expr('keyword{keyword}',
                  'Token keyword')

        decl_expr('oper{oper}, right{std::move(right)}',
                  'Token oper, expr_node_t<T> right')

        decl_expr('name{name}',
                  'Token name')

        file.write('// End of expression node definitions\n\n')
        file.write('// Statement node definitions\n\n')

        decl_stmt('stmts{std::move(stmts)}',
                  'std::vector<stmt_node_t<T>> stmts')

        decl_stmt('keyword{keyword}',
                  'Token keyword')

        file.write('enum class VisibilityType {\n')
        tab(file, 1).write('PRIVATE,\n')
        tab(file, 1).write('PROTECTED,\n')
        tab(file, 1).write('PUBLIC\n')
        file.write('};\n\n')

        decl_stmt('name{name}, members{std::move(members)}, methods{std::move(methods)}',
                  'Token name, std::vector<std::pair<stmt_node_t<T>,VisibilityType>> members, ' +
                  'std::vector<std::pair<stmt_node_t<T>,VisibilityType>> methods')

        decl_stmt('keyword{keyword}',
                  'Token keyword')

        decl_stmt('expr{std::move(expr)}',
                  'expr_node_t<T> expr')

        decl_stmt('name{name}, return_type{std::move(return_type)}, params{std::move(params)}, body{std::move(body)}',
                  'Token name, type_node_t<T> return_type, std::vector<std::pair<Token,type_node_t<T>>> params, ' +
                  'std::vector<stmt_node_t<T>> body')

        decl_stmt('condition{std::move(condition)}, thenBranch{std::move(thenBranch)},' +
                  'elseBranch{std::move(elseBranch)}',
                  'expr_node_t<T> condition, stmt_node_t<T> thenBranch, stmt_node_t<T> elseBranch')

        decl_stmt('name{name}',
                  'Token name')

        decl_stmt('keyword{keyword}, value{std::move(value)}',
                  'Token keyword, expr_node_t<T> value')

        decl_stmt('condition{std::move(condition)}, cases{std::move(cases)}, default_case{std::move(default_case)}',
                  'expr_node_t<T> condition, std::vector<std::pair<expr_node_t<T>,stmt_node_t<T>>> cases, ' +
                  'std::optional<std::pair<expr_node_t<T>,stmt_node_t<T>>> default_case')

        decl_stmt('name{name}, type{std::move(type)}',
                  'Token name, type_node_t<T> type')

        decl_stmt('name{name}, type{std::move(type)}, initializer{std::move(initializer)}',
                  'Token name, type_node_t<T> type, expr_node_t<T> initializer')

        decl_stmt('condition{std::move(condition)}, body{std::move(body)}',
                  'expr_node_t<T> condition, stmt_node_t<T> body')

        file.write('// End of statement node definitions\n\n')

        # varDecl, funcDecl, arguments

        end_file(file)
