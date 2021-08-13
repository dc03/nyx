# Copyright (C) 2021  Dhruv Chawla
# See LICENSE at project root for license details
from typing import *


def make_header(file, guard: str) -> None:
    file.write('#pragma once\n\n')
    file.write('/* Copyright (C) 2021  Dhruv Chawla */\n')
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
    file.write('using ' + alias + ' = ' + name + ';\n')
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


def declare_base(file, base_name: str, members: str = '', ctor_args: str = '') -> None:
    file.write('struct ' + base_name + ' {\n')

    member_list = members.split(', ')
    if members != '':
        for member in member_list:
            tab(file, 1).write(member.strip() + '{};\n')
        file.write('\n')

    tab(file, 1).write(base_name + '() = default;\n')
    if ctor_args != '':
        tab(file, 1).write(base_name + '(' + members + '): ' + ctor_args + '{}\n')
    tab(file, 1).write('virtual std::string_view string_tag() = 0;\n')
    tab(file, 1).write('virtual NodeType type_tag() = 0;\n')
    tab(file, 1).write('virtual ' + base_name + 'VisitorType accept(Visitor &visitor) = 0;\n')
    tab(file, 1).write('virtual ~' + base_name + '() = default;\n')
    file.write('};\n\n')
    return None


def declare_derived_type(file, base_name: str, derived_name: str, ctor_args: str, members: str, ctor_params: str,
                         typedefs: List[str] = None) -> None:
    file.write('struct ' + derived_name +
               ' final: public ' + base_name + ' {\n')

    if typedefs is not None:
        for typedef in typedefs:
            tab(file, 1).write(typedef.strip() + ';\n')
    file.write('\n')

    member_list = members.split(', ')
    if members != '':
        for member in member_list:
            tab(file, 1).write(member.strip() + '{};\n')
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

    tab(file, 1).write(derived_name + '() = default;\n')
    tab(file, 1).write(('explicit ' if len(member_list) == 1 else '') + derived_name + '(' + ctor_params + ')' + (
        ':\n' if ctor_args != '' else '\n'))
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


def declare_type_type(name: str, ctor_args: str, members: str, ctor_params: str, typedefs: List[str] = None) -> None:
    declare_derived_type(file, 'BaseType', name + 'Type', ctor_args, members, ctor_params, typedefs)
    return None


def declare_expr_type(name: str, ctor_args: str, members: str, typedefs: List[str] = None) -> None:
    declare_derived_type(file, 'Expr', name + 'Expr', ctor_args, members, members, typedefs)
    return None


def declare_stmt_type(name: str, ctor_args: str, members: str, typedefs: List[str] = None) -> None:
    declare_derived_type(file, 'Stmt', name + 'Stmt', ctor_args, members, members, typedefs)
    return None


if __name__ == '__main__':
    with open('AST.hpp', 'wt') as file:
        make_header(file, 'AST_HPP')
        file.write('#include "Token.hpp"\n')
        file.write('#include "VisitorTypes.hpp"\n\n')

        file.write('#include <memory>\n')
        file.write('#include <string>\n')
        file.write('#include <string_view>\n')
        file.write('#include <tuple>\n')
        file.write('#include <vector>\n\n')
        forward_declare(file, ['Expr', 'Stmt', 'BaseType'])
        file.write('\n')
        declare_alias(file, 'ExprNode', 'std::unique_ptr<Expr>')
        declare_alias(file, 'StmtNode', 'std::unique_ptr<Stmt>')
        declare_alias(file, 'TypeNode', 'std::unique_ptr<BaseType>')
        file.write('\n')
        declare_alias(file, 'RequiresCopy', 'bool')
        # Base class and alias declarations complete

        Exprs: List[str] = ['Assign', 'Binary', 'Call', 'Comma', 'Get', 'Grouping', 'Index', 'List', 'ListAssign',
                            'Literal', 'Logical', 'Move', 'ScopeAccess', 'ScopeName', 'Set', 'Super', 'Ternary', 'This',
                            'Tuple', 'Unary', 'Variable']
        Stmts: List[str] = ['Block', 'Break', 'Class', 'Continue', 'Expression', 'Function',
                            'If', 'Return', 'Switch', 'Type', 'Var', 'While']
        Types: List[str] = ['Primitive', 'UserDefined', 'List', 'Tuple', 'Typeof']

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

        declare_base(file, 'BaseType',
                     'Type primitive, bool is_const, bool is_ref',
                     'primitive{primitive}, is_const{is_const}, is_ref{is_ref}')
        declare_base(file, 'Expr',
                     'ExprVisitorType resolved')
        declare_base(file, 'Stmt')

        # Type base node definition

        # NOTE: Always make sure that these are in the same order as the lists

        file.write('// Type node definitions\n\n')

        declare_type_type('Primitive',
                          'BaseType{primitive, is_const, is_ref}',
                          '',
                          'Type primitive, bool is_const, bool is_ref')

        declare_type_type('UserDefined',
                          'BaseType{primitive, is_const, is_ref}, name{std::move(name)}',
                          'Token name',
                          'Type primitive, bool is_const, bool is_ref, Token name')

        declare_type_type('List',
                          'BaseType{primitive, is_const, is_ref}, contained{std::move(contained)}, size{std::move('
                          'size)}',
                          'TypeNode contained, ExprNode size',
                          'Type primitive, bool is_const, bool is_ref, TypeNode contained, ExprNode size')

        declare_type_type('Tuple',
                          'BaseType{primitive, is_const, is_ref}, types{std::move(types)}',
                          'std::vector<TypeNode> types',
                          'Type primitive, bool is_const, bool is_ref, std::vector<TypeNode> types')

        declare_type_type('Typeof',
                          'BaseType{primitive, is_const, is_ref}, expr{std::move(expr)}',
                          'ExprNode expr',
                          'Type primitive, bool is_const, bool is_ref, ExprNode expr')

        file.write('// End of type node definitions\n\n')

        file.write('// Expression node definitions\n\n')

        file.write('enum class NumericConversionType {\n')
        tab(file, 1).write('FLOAT_TO_INT,\n')
        tab(file, 1).write('INT_TO_FLOAT,\n')
        tab(file, 1).write('NONE\n')
        file.write('};\n\n')

        file.write('enum class IdentifierType {\n')
        tab(file, 1).write('LOCAL,\n')
        tab(file, 1).write('GLOBAL,\n')
        tab(file, 1).write('FUNCTION,\n')
        tab(file, 1).write('CLASS\n')
        file.write('};\n\n')

        declare_expr_type('Assign',
                          'target{std::move(target)}, value{std::move(value)}, conversion_type{conversion_type},'
                          'requires_copy{requires_copy}, target_type{target_type}',
                          'Token target, ExprNode value, NumericConversionType conversion_type, RequiresCopy '
                          'requires_copy, IdentifierType target_type')

        declare_expr_type('Binary',
                          'left{std::move(left)}, right{std::move(right)}',
                          'ExprNode left, ExprNode right')

        declare_expr_type('Call',
                          'function{std::move(function)}, args{std::move(args)}, is_native_call{is_native_call}',
                          'ExprNode function, std::vector<std::tuple<ExprNode,NumericConversionType,RequiresCopy>> '
                          'args, bool is_native_call')

        declare_expr_type('Comma',
                          'exprs{std::move(exprs)}',
                          'std::vector<ExprNode> exprs')

        declare_expr_type('Get',
                          'object{std::move(object)}, name{std::move(name)}',
                          'ExprNode object, Token name')

        declare_expr_type('Grouping',
                          'expr{std::move(expr)}, type{std::move(type)}',
                          'ExprNode expr, TypeNode type')

        declare_expr_type('Index',
                          'object{std::move(object)}, index{std::move(index)}',
                          'ExprNode object, ExprNode index')

        declare_expr_type('List',
                          'bracket{std::move(bracket)}, elements{std::move(elements)}, type{std::move(type)}',
                          'Token bracket, std::vector<ElementType> elements, std::unique_ptr<ListType> type',
                          ['using ElementType = std::tuple<ExprNode, NumericConversionType, RequiresCopy>'])

        declare_expr_type('ListAssign',
                          'list{std::move(list)}, value{std::move(value)}, conversion_type{conversion_type}, '
                          'requires_copy{requires_copy}',
                          'IndexExpr list, ExprNode value, NumericConversionType conversion_type, RequiresCopy '
                          'requires_copy')

        declare_expr_type('Literal',
                          'value{std::move(value)}, type{std::move(type)}',
                          'LiteralValue value, TypeNode type')

        declare_expr_type('Logical',
                          'left{std::move(left)}, right{std::move(right)}',
                          'ExprNode left, ExprNode right')

        declare_expr_type('Move',
                          'expr{std::move(expr)}',
                          'ExprNode expr')

        declare_expr_type('ScopeAccess',
                          'scope{std::move(scope)}, name{std::move(name)}',
                          'ExprNode scope, Token name')

        declare_expr_type('ScopeName',
                          'name{std::move(name)}',
                          'Token name')

        declare_expr_type('Set',
                          'object{std::move(object)}, name{std::move(name)}, value{std::move(value)}, '
                          'conversion_type{conversion_type}, requires_copy{requires_copy}',
                          'ExprNode object, Token name, ExprNode value, NumericConversionType conversion_type, '
                          'RequiresCopy requires_copy')

        declare_expr_type('Super',
                          'keyword{std::move(keyword)}, name{std::move(name)}',
                          'Token keyword, Token name')

        declare_expr_type('Ternary',
                          'left{std::move(left)}, middle{std::move(middle)}, right{std::move(right)}',
                          'ExprNode left, ExprNode middle, ExprNode right')

        declare_expr_type('This',
                          'keyword{std::move(keyword)}',
                          'Token keyword')

        declare_expr_type('Tuple',
                          'brace{std::move(brace)}, elements{std::move(elements)}, type{std::move(type)}',
                          'Token brace, std::vector<ElementType> elements, std::unique_ptr<TupleType> type',
                          ['using ElementType = std::tuple<ExprNode, NumericConversionType, RequiresCopy>'])

        declare_expr_type('Unary',
                          'oper{std::move(oper)}, right{std::move(right)}',
                          'Token oper, ExprNode right')

        declare_expr_type('Variable',
                          'name{std::move(name)}, type{type}',
                          'Token name, IdentifierType type')

        file.write('// End of expression node definitions\n\n')
        file.write('// Statement node definitions\n\n')

        declare_stmt_type('Block',
                          'stmts{std::move(stmts)}',
                          'std::vector<StmtNode> stmts')

        declare_stmt_type('Break',
                          'keyword{std::move(keyword)}',
                          'Token keyword')

        file.write('enum class VisibilityType {\n')
        tab(file, 1).write('PRIVATE,\n')
        tab(file, 1).write('PROTECTED,\n')
        tab(file, 1).write('PUBLIC\n')
        file.write('};\n\n')

        declare_stmt_type('Class',
                          'name{std::move(name)}, ctor{ctor}, dtor{dtor}, members{std::move(members)}, methods{'
                          'std::move(methods)}',
                          'Token name, FunctionStmt *ctor, FunctionStmt *dtor, std::vector<MemberType> members, '
                          'std::vector<MethodType> methods',
                          ['using MemberType = std::pair<std::unique_ptr<VarStmt>,VisibilityType>',
                           'using MethodType = std::pair<std::unique_ptr<FunctionStmt>,VisibilityType>'])

        declare_stmt_type('Continue',
                          'keyword{std::move(keyword)}',
                          'Token keyword')

        declare_stmt_type('Expression',
                          'expr{std::move(expr)}',
                          'ExprNode expr')

        declare_stmt_type('Function',
                          'name{std::move(name)}, return_type{std::move(return_type)}, params{std::move(params)}, '
                          'body{std::move(body)}, return_stmts{std::move(return_stmts)}, scope_depth{scope_depth}',
                          'Token name, TypeNode return_type, std::vector<std::pair<Token,TypeNode>> params, '
                          'StmtNode body, std::vector<ReturnStmt*> return_stmts, std::size_t scope_depth')

        declare_stmt_type('If',
                          'keyword{std::move(keyword)}, condition{std::move(condition)}, thenBranch{std::move('
                          'thenBranch)}, elseBranch{std::move(elseBranch)}',
                          'Token keyword, ExprNode condition, StmtNode thenBranch, StmtNode elseBranch')

        declare_stmt_type('Return',
                          'keyword{std::move(keyword)}, value{std::move(value)}, locals_popped{locals_popped}, '
                          'function{function}',
                          'Token keyword, ExprNode value, std::size_t locals_popped, FunctionStmt *function')

        declare_stmt_type('Switch',
                          'condition{std::move(condition)}, cases{std::move(cases)}, default_case{std::move('
                          'default_case)}',
                          'ExprNode condition, std::vector<std::pair<ExprNode,StmtNode>> cases, StmtNode default_case')

        declare_stmt_type('Type',
                          'name{std::move(name)}, type{std::move(type)}',
                          'Token name, TypeNode type')

        declare_stmt_type('Var',
                          'keyword{std::move(keyword)}, name{std::move(name)}, type{std::move(type)}, initializer{'
                          'std::move(initializer)}, conversion_type{conversion_type}, requires_copy{requires_copy}',
                          'Token keyword, Token name, TypeNode type, ExprNode initializer, NumericConversionType '
                          'conversion_type, RequiresCopy requires_copy')

        declare_stmt_type('While',
                          'keyword{std::move(keyword)}, condition{std::move(condition)}, body{std::move(body)}, '
                          'increment{std::move(increment)}',
                          'Token keyword, ExprNode condition, StmtNode body, StmtNode increment')

        file.write('// End of statement node definitions\n\n')

        file.write('// Helper function to turn a given type node into a string\n')
        file.write('std::string stringify(BaseType *node);\n\n')
        file.write('// Helper function to copy a given type node (list size expressions are not copied however)\n')
        file.write('BaseTypeVisitorType copy_type(BaseType *node);\n\n')

        end_file(file)
