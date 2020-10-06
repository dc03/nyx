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
        file.write('struct ' + name + ';\n')


def declare_alias(file, alias: str, name: str) -> None:
    file.write('using ' + alias + ' = std::unique_ptr<' + name + '>;\n')
    return None


def tab(file, n: int):
    file.write('    ' * n)
    return file


def declare_visitor(file, visitor: str, exprs: List[str], stmts: List[str], expr_base: str, stmt_base: str,
                    types: List[str], type_base: str) -> None:
    file.write('struct ' + visitor + ' {\n')
    for expr in exprs:
        tab(file, 1).write(
            'virtual T visit(' + expr + '& ' + expr_base.lower() + ') = 0;\n')

    file.write('\n')

    for stmt in stmts:
        tab(file, 1).write(
            'virtual T visit(' + stmt + '& ' + stmt_base.lower() + ') = 0;\n')

    file.write('\n')

    for type in types:
        tab(file, 1).write(
            'virtual T visit(' + type + '& ' + type_base.lower() + ') = 0;\n')

    file.write('};\n\n')
    return None


def declare_base(file, base_name: str) -> None:
    file.write('struct ' + base_name + ' {\n')
    tab(file, 1).write('virtual std::string_view string_tag() = 0;\n')
    tab(file, 1).write('virtual NodeType type_tag() = 0;\n')
    tab(file, 1).write('virtual T accept(Visitor& visitor) = 0;\n')
    tab(file, 1).write('virtual ~' + base_name + '() = default;\n')
    file.write('};\n\n')
    return None


def declare_derived(file, base_name: str, derived_name: str, ctor_args: str, members: str) -> None:
    file.write('struct ' + derived_name +
               ' final: public ' + base_name + ' {\n')

    if members != '':
        for member in members.split(', '):
            tab(file, 1).write(member.strip() + ';\n')
        file.write('\n')
        # Class members

    tab(file, 1).write('std::string_view string_tag() override final {\n')
    tab(file, 2).write('return "' + derived_name + '";\n')
    tab(file, 1).write('}\n\n')
    # string_tag() method

    tab(file, 1).write('NodeType type_tag() override final {\n')
    tab(file, 2).write('return NodeType::' + derived_name + ';\n')
    tab(file, 1).write('}\n\n')
    # type_tag() method

    tab(file, 1).write(derived_name + '(' + members + ')' + (':\n' if ctor_args != '' else '\n'))
    tab(file, 2).write(ctor_args + ' {}\n')
    file.write('\n')
    # Class constructor

    tab(file, 1).write('T accept(Visitor& visitor) override final {\n')
    tab(file, 2).write('return visitor.visit(*this);\n')
    tab(file, 1).write('}\n')
    # Accept method
    file.write('};\n\n')
    return None


def declare_tag_enum(file, exprs: List[str], stmts: List[str], types: List[str]) -> None:
    file.write('enum class NodeType {\n')
    for expr in exprs:
        tab(file, 1).write(expr + ',\n')
    file.write('\n')

    for stmt in stmts:
        tab(file, 1).write(stmt + ',\n')
    file.write('\n')

    for i in range(len(types)):
        tab(file, 1).write(types[i])
        if i < len(types) - 1:
            file.write(',')
        file.write('\n')

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
        file.write('#include <string>\n')
        file.write('#include <string_view>\n')
        file.write('#include <utility>\n')
        file.write('#include <vector>\n')
        file.write('\n#include "Parser/Object.hpp"\n')
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
        Types: List[str] = ['Primitive', 'UserDefined', 'List', 'Typeof']

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
        declare_tag_enum(file, Exprs, Stmts, Types)
        # Visitor declaration complete

        declare_base(file, 'Expr')
        declare_base(file, 'Stmt')

        file.write('struct Type {\n')
        tab(file, 1).write('virtual std::string_view string_tag() = 0;\n')
        tab(file, 1).write('virtual NodeType type_tag() = 0;\n')
        tab(file, 1).write('virtual T accept(Visitor& visitor) = 0;\n')
        tab(file, 1).write('virtual ~Type() = default;\n')
        file.write('};\n\n')

        file.write('struct SharedData {\n')
        tab(file, 1).write('TypeType type;\n')
        tab(file, 1).write('bool is_const;\n')
        tab(file, 1).write('bool is_ref;\n\n')
        file.write('};\n\n')
        # Type base node definition

        # NOTE: Always make sure that these are in the same order as the lists

        file.write('// Type node definitions\n\n')

        declare_derived(file, 'Type', Types[0], 'data{data}', 'SharedData data')

        declare_derived(file, 'Type', Types[1], 'data{data}, name{name}',
                        'SharedData data, Token name')

        declare_derived(file, 'Type', Types[2],
                        'data{data}, contained{std::move(contained)}, size{std::move(size)}',
                        'SharedData data, type_node_t contained, expr_node_t size')

        declare_derived(file, 'Type', Types[3],
                        'expr{std::move(expr)}', 'expr_node_t expr')

        file.write('// End of type node definitions\n\n')

        file.write('// Expression node definitions\n\n')

        decl_expr('target{target}, value{std::move(value)}',
                  'Token target, expr_node_t value')

        decl_expr('left{std::move(left)}, oper{oper}, right{std::move(right)}',
                  'expr_node_t left, Token oper, expr_node_t right')

        decl_expr('function{std::move(function)}, paren{paren}, args{std::move(args)}',
                  'expr_node_t function, Token paren, std::vector<expr_node_t> args')

        decl_expr('exprs{std::move(exprs)}',
                  'std::vector<expr_node_t> exprs')

        decl_expr('object{std::move(object)}, name{name}',
                  'expr_node_t object, Token name')

        decl_expr('expr{std::move(expr)}',
                  'expr_node_t expr')

        decl_expr('object{std::move(object)}, oper{oper}, index{std::move(index)}',
                  'expr_node_t object, Token oper, expr_node_t index')

        decl_expr('value{std::move(value)}',
                  'T value')

        decl_expr('left{std::move(left)}, oper{oper}, right{std::move(right)}',
                  'expr_node_t left, Token oper, expr_node_t right')

        decl_expr('object{std::move(object)}, name{name}, value{std::move(value)}',
                  'expr_node_t object, Token name, expr_node_t value')

        decl_expr('keyword{keyword}, name{name}',
                  'Token keyword, Token name')

        decl_expr('left{std::move(left)}, question{question}, middle{std::move(middle)}, right{std::move(right)}',
                  'expr_node_t left, Token question, expr_node_t middle, expr_node_t right')

        decl_expr('keyword{keyword}',
                  'Token keyword')

        decl_expr('oper{oper}, right{std::move(right)}',
                  'Token oper, expr_node_t right')

        decl_expr('name{name}',
                  'Token name')

        file.write('// End of expression node definitions\n\n')
        file.write('// Statement node definitions\n\n')

        decl_stmt('stmts{std::move(stmts)}',
                  'std::vector<stmt_node_t> stmts')

        decl_stmt('keyword{keyword}',
                  'Token keyword')

        file.write('enum class VisibilityType {\n')
        tab(file, 1).write('PRIVATE,\n')
        tab(file, 1).write('PROTECTED,\n')
        tab(file, 1).write('PUBLIC,\n')
        tab(file, 1).write('PRIVATE_DTOR,\n')
        tab(file, 1).write('PROTECTED_DTOR,\n')
        tab(file, 1).write('PUBLIC_DTOR\n')
        file.write('};\n\n')

        decl_stmt('name{name}, has_ctor{has_ctor}, has_dtor{has_dtor}, members{std::move(members)}, ' +
                  'methods{std::move(methods)}',
                  'Token name, bool has_ctor, bool has_dtor, std::vector<std::pair<stmt_node_t,' +
                  'VisibilityType>> members, std::vector<std::pair<stmt_node_t,VisibilityType>> methods')

        decl_stmt('keyword{keyword}',
                  'Token keyword')

        decl_stmt('expr{std::move(expr)}',
                  'expr_node_t expr')

        decl_stmt('name{name}, return_type{std::move(return_type)}, params{std::move(params)}, body{std::move(body)}',
                  'Token name, type_node_t return_type, std::vector<std::pair<Token,type_node_t>> params, ' +
                  'stmt_node_t body')

        decl_stmt('condition{std::move(condition)}, thenBranch{std::move(thenBranch)},' +
                  'elseBranch{std::move(elseBranch)}',
                  'expr_node_t condition, stmt_node_t thenBranch, stmt_node_t elseBranch')

        decl_stmt('name{name}',
                  'Token name')

        decl_stmt('keyword{keyword}, value{std::move(value)}',
                  'Token keyword, expr_node_t value')

        decl_stmt('condition{std::move(condition)}, cases{std::move(cases)}, default_case{std::move(default_case)}',
                  'expr_node_t condition, std::vector<std::pair<expr_node_t,stmt_node_t>> cases, ' +
                  'std::optional<stmt_node_t> default_case')

        decl_stmt('name{name}, type{std::move(type)}',
                  'Token name, type_node_t type')

        decl_stmt('name{name}, type{std::move(type)}, initializer{std::move(initializer)}',
                  'Token name, type_node_t type, expr_node_t initializer')

        decl_stmt('condition{std::move(condition)}, body{std::move(body)}',
                  'expr_node_t condition, stmt_node_t body')

        file.write('// End of statement node definitions\n\n')

        end_file(file)
