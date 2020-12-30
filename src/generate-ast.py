# Copyright (C) 2020  Dhruv Chawla
# See LICENSE at project root for license details
from typing import *


def make_header(file, guard: str) -> None:
    file.write('#pragma once\n\n')
    file.write('/* Copyright (C) 2020  Dhruv Chawla */\n')
    file.write('/* See LICENSE at project root for license details */\n\n')
    file.write('#ifndef ' + guard + '\n')
    file.write('#define ' + guard + '\n\n')
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
            'virtual ' + expr_base + 'VisitorType visit(' + expr + ' &' + expr_base.lower() + ') = 0;\n')

    file.write('\n')

    for stmt in stmts:
        tab(file, 1).write(
            'virtual ' + stmt_base + 'VisitorType visit(' + stmt + ' &' + stmt_base.lower() + ') = 0;\n')

    file.write('\n')

    for type in types:
        tab(file, 1).write(
            'virtual ' + type_base + 'VisitorType visit(' + type + ' &' + type_base.lower() + ') = 0;\n')

    file.write('};\n\n')
    return None


def declare_base(file, base_name: str) -> None:
    file.write('struct ' + base_name + ' {\n')
    tab(file, 1).write('virtual std::string_view string_tag() = 0;\n')
    tab(file, 1).write('virtual NodeType type_tag() = 0;\n')
    tab(file, 1).write('virtual ' + base_name + 'VisitorType accept(Visitor &visitor) = 0;\n')
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

    tab(file, 1).write(base_name + 'VisitorType accept(Visitor &visitor) override final {\n')
    tab(file, 2).write('return visitor.visit(*this);\n')
    tab(file, 1).write('}\n')
    # Accept method
    file.write('};\n\n')
    return None


def declare_derived_type(file, base_name: str, derived_name: str, ctor_args: str, members: str, ctor_params: str) \
        -> None:
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

    tab(file, 1).write(derived_name + '(' + ctor_params + ')' + (':\n' if ctor_args != '' else '\n'))
    tab(file, 2).write(ctor_args + ' {}\n')
    file.write('\n')
    # Class constructor

    tab(file, 1).write(base_name + 'VisitorType accept(Visitor &visitor) override final {\n')
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
        file.write('#include "Parser/VisitorTypes.hpp"\n')
        file.write('#include "Token.hpp"\n\n')
        file.write('#include <memory>\n')
        file.write('#include <optional>\n')
        file.write('#include <string>\n')
        file.write('#include <string_view>\n')
        file.write('#include <tuple>\n')
        file.write('#include <vector>\n\n')
        forward_declare(file, ['Expr', 'Stmt', 'BaseType'])
        file.write('\n')
        declare_alias(file, 'ExprNode', 'Expr')
        declare_alias(file, 'StmtNode', 'Stmt')
        declare_alias(file, 'TypeNode', 'BaseType')
        # Base class and alias declarations complete

        Exprs: List[str] = ['Assign', 'Binary', 'Call', 'Comma', 'Get', 'Grouping', 'Index', 'Literal',
                            'Logical', 'ScopeAccess', 'ScopeName', 'Set', 'Super', 'Ternary', 'This', 'Unary',
                            'Variable']
        Stmts: List[str] = ['Block', 'Break', 'Class', 'Continue', 'Expression', 'Function',
                            'If', 'Return', 'Switch', 'Type', 'Var', 'While']
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

        declare_visitor(file, 'Visitor', Exprs, Stmts, 'Expr', 'Stmt', Types, 'BaseType')
        declare_tag_enum(file, Exprs, Stmts, Types)
        # Visitor declaration complete

        declare_base(file, 'Expr')
        declare_base(file, 'Stmt')

        file.write('struct SharedData {\n')
        tab(file, 1).write('Type type;\n')
        tab(file, 1).write('bool is_const;\n')
        tab(file, 1).write('bool is_ref;\n\n')
        file.write('};\n\n')

        file.write('struct BaseType {\n')
        tab(file, 1).write('SharedData data;\n\n')
        tab(file, 1).write('BaseType() = default;\n')
        tab(file, 1).write('BaseType(SharedData data): data{data} {}\n')
        tab(file, 1).write('virtual std::string_view string_tag() = 0;\n')
        tab(file, 1).write('virtual NodeType type_tag() = 0;\n')
        tab(file, 1).write('virtual BaseTypeVisitorType accept(Visitor& visitor) = 0;\n')
        tab(file, 1).write('virtual ~BaseType() = default;\n')
        file.write('};\n\n')

        # Type base node definition

        # NOTE: Always make sure that these are in the same order as the lists

        file.write('// Type node definitions\n\n')

        declare_derived_type(file, 'BaseType', Types[0], 'BaseType{data}', '', 'SharedData data')

        declare_derived_type(file, 'BaseType', Types[1], 'BaseType{data}, name{name}',
                             'Token name', 'SharedData data, Token name')

        declare_derived_type(file, 'BaseType', Types[2],
                             'BaseType{data}, contained{std::move(contained)}, size{std::move(size)}',
                             'TypeNode contained, ExprNode size',
                             'SharedData data, TypeNode contained, ExprNode size')

        declare_derived_type(file, 'BaseType', Types[3],
                             'BaseType{data}, expr{std::move(expr)}', 'ExprNode expr',
                             'SharedData data, ExprNode expr')

        file.write('// End of type node definitions\n\n')

        file.write('// Expression node definitions\n\n')

        file.write('enum class NumericConversionType {\n')
        tab(file, 1).write('FLOAT_TO_INT,\n')
        tab(file, 1).write('INT_TO_FLOAT,\n')
        tab(file, 1).write('NONE\n')
        file.write('};\n\n')

        decl_expr('target{target}, value{std::move(value)}, conversion_type{conversion_type}, requires_copy{' +
                  'requires_copy}, stack_slot{stack_slot}',
                  'Token target, ExprNode value, NumericConversionType conversion_type, bool requires_copy, std::size_t'
                  + ' stack_slot')

        decl_expr('left{std::move(left)}, oper{oper}, right{std::move(right)}, resolved_type{resolved_type}',
                  'ExprNode left, Token oper, ExprNode right, ExprVisitorType resolved_type')

        decl_expr('function{std::move(function)}, paren{paren}, args{std::move(args)}',
                  'ExprNode function, Token paren, std::vector<std::tuple<ExprNode,NumericConversionType,bool>> args')

        decl_expr('exprs{std::move(exprs)}',
                  'std::vector<ExprNode> exprs')

        decl_expr('object{std::move(object)}, name{name}',
                  'ExprNode object, Token name')

        decl_expr('expr{std::move(expr)}',
                  'ExprNode expr')

        decl_expr('object{std::move(object)}, oper{oper}, index{std::move(index)}',
                  'ExprNode object, Token oper, ExprNode index')

        decl_expr('value{std::move(value)}, lexeme{std::move(lexeme)}, type{std::move(type)}',
                  'LiteralValue value, Token lexeme, TypeNode type')

        decl_expr('left{std::move(left)}, oper{oper}, right{std::move(right)}',
                  'ExprNode left, Token oper, ExprNode right')

        decl_expr('scope{std::move(scope)}, name{name}',
                  'ExprNode scope, Token name')

        decl_expr('name{name}',
                  'Token name')

        decl_expr('object{std::move(object)}, name{name}, value{std::move(value)}, conversion_type{conversion_type}, ' +
                  'requires_copy{requires_copy}',
                  'ExprNode object, Token name, ExprNode value, NumericConversionType conversion_type, bool ' +
                  'requires_copy')

        decl_expr('keyword{keyword}, name{name}',
                  'Token keyword, Token name')

        decl_expr('left{std::move(left)}, question{question}, middle{std::move(middle)}, right{std::move(right)}',
                  'ExprNode left, Token question, ExprNode middle, ExprNode right')

        decl_expr('keyword{keyword}',
                  'Token keyword')

        decl_expr('oper{oper}, right{std::move(right)}',
                  'Token oper, ExprNode right')

        decl_expr('name{name}, stack_slot{stack_slot}',
                  'Token name, std::size_t stack_slot')

        file.write('// End of expression node definitions\n\n')
        file.write('// Statement node definitions\n\n')

        decl_stmt('stmts{std::move(stmts)}',
                  'std::vector<StmtNode> stmts')

        decl_stmt('keyword{keyword}',
                  'Token keyword')

        file.write('enum class VisibilityType {\n')
        tab(file, 1).write('PRIVATE,\n')
        tab(file, 1).write('PROTECTED,\n')
        tab(file, 1).write('PUBLIC\n')
        file.write('};\n\n')

        decl_stmt('name{name}, ctor{ctor}, dtor{dtor}, members{std::move(members)}, ' +
                  'methods{std::move(methods)}',
                  'Token name, FunctionStmt *ctor, FunctionStmt *dtor, std::vector<std::pair<std::unique_ptr<VarStmt>,'
                  + 'VisibilityType>> members, std::vector<std::pair<std::unique_ptr<FunctionStmt>,VisibilityType>>'
                  + 'methods')

        decl_stmt('keyword{keyword}',
                  'Token keyword')

        decl_stmt('expr{std::move(expr)}',
                  'ExprNode expr')

        decl_stmt('name{name}, return_type{std::move(return_type)}, params{std::move(params)}, body{std::move(body)}',
                  'Token name, TypeNode return_type, std::vector<std::pair<Token,TypeNode>> params, ' +
                  'StmtNode body')

        decl_stmt('keyword{keyword}, condition{std::move(condition)}, thenBranch{std::move(thenBranch)},' +
                  'elseBranch{std::move(elseBranch)}',
                  'Token keyword, ExprNode condition, StmtNode thenBranch, StmtNode elseBranch')

        decl_stmt('keyword{keyword}, value{std::move(value)}',
                  'Token keyword, ExprNode value')

        decl_stmt('condition{std::move(condition)}, cases{std::move(cases)}, default_case{std::move(default_case)}',
                  'ExprNode condition, std::vector<std::pair<ExprNode,StmtNode>> cases, ' +
                  'StmtNode default_case')

        decl_stmt('name{name}, type{std::move(type)}',
                  'Token name, TypeNode type')

        decl_stmt('is_val{is_val}, name{name}, type{std::move(type)}, initializer{std::move(initializer)},' +
                  'conversion_type{conversion_type}, requires_copy{requires_copy}, init_is_ref{init_is_ref}',
                  'bool is_val, Token name, TypeNode type, ExprNode initializer, NumericConversionType conversion_type'
                  + ', bool requires_copy, bool init_is_ref')

        decl_stmt('keyword{keyword}, condition{std::move(condition)}, body{std::move(body)}, increment{std::move(' +
                  'increment)}',
                  'Token keyword, ExprNode condition, StmtNode body, StmtNode increment')

        file.write('// End of statement node definitions\n\n')

        file.write('// Helper function to turn a given type node into a string\n')
        file.write('std::string stringify(BaseType *node);\n\n')
        file.write('// Helper function to copy a given type node (list size expressions are not copied however)\n')
        file.write('BaseTypeVisitorType copy_type(BaseType *node);\n\n')

        end_file(file)
