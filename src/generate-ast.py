def make_header(file, guard):
    file.write('#pragma once\n\n')
    file.write('#ifndef ' + guard + '\n')
    file.write('#  define ' + guard + '\n\n')


def end_file(file):
    file.write('#endif\n')


def forward_declare(file, names):
    for name in names:
        file.write('template <typename T>\n')
        file.write('struct ' + name + ';\n')


def declare_alias(file, alias, name):
    file.write('template <typename T>\n')
    file.write('using ' + alias + ' = std::unique_ptr<' + name + '<T>>;\n')


def tab(file, n):
    file.write('\t' * n)
    return file


def declare_visitor(file, visitor, exprs, stmts, expr_base, stmt_base):
    file.write('template <typename T>\n')
    file.write('struct ' + visitor + ' {\n')
    for expr in exprs:
        tab(file, 1).write(
            'T visit(' + expr + '<T>& ' + expr_base.lower() + ') = 0;\n')

    for stmt in stmts:
        tab(file, 1).write(
            'T visit(' + stmt + '<T>& ' + stmt_base.lower() + ') = 0;\n')
    file.write('};\n\n')


def declare_base(file, base_name):
    file.write('template <typename T>\n')
    file.write('struct ' + base_name + ' {\n')
    tab(file, 1).write('virtual T accept(Visitor<T>& visitor) = 0;\n')
    tab(file, 1).write('virtual ~' + base_name + '() = default;\n')
    file.write('};\n\n')

def declare_derived(file, base_name, derived_name, ctor_args, members):
    file.write('template <typename T>\n')
    file.write('struct ' + derived_name + ' final: public ' + base_name + '<T> {\n')

    for member in members.split(','):
        tab(file, 1).write(member.strip() + ';\n')
    file.write('\n')
    # Class members

    tab(file, 1).write(derived_name + '(' + members + '):\n')
    tab(file, 2).write(ctor_args + ' {}\n')
    file.write('\n')
    # Class constructor

    tab(file, 1).write('T accept(Visitor<T>& visitor) override final {\n')
    tab(file, 2).write('return visitor.visit(*this);\n')
    tab(file, 1).write('}\n')
    # Accept method
    file.write('};\n\n')

if __name__ == '__main__':
    with open('AST.hpp', 'wt') as file:
        make_header(file, 'AST_HPP')
        file.write('#include <memory>\n')
        file.write('#include <vector>\n')
        file.write('#include "Token.hpp"\n\n')
        forward_declare(file, ['Expr', 'Stmt'])
        file.write('\n')
        declare_alias(file, 'expr_node_t', 'Expr')
        declare_alias(file, 'stmt_node_t', 'Stmt')
        # Base class and alias declarations complete

        Exprs = ['Assign', 'Binary', 'Call', 'Get', 'Grouping', 'Literal',
                 'Logical', 'Set', 'Super', 'This', 'Unary', 'Variable']
        Stmts = ['Block', 'Break', 'Class', 'Continue', 'Expression', 'For',
                 'Func', 'If', 'Import', 'Return', 'Switch', 'Type', 'Var',  'While']
        Exprs = [x + 'Expr' for x in Exprs]
        Stmts = [x + 'Stmt' for x in Stmts]
        file.write('\n// Expression nodes\n\n')
        forward_declare(file, Exprs)
        file.write('\n// Statement nodes\n\n')
        forward_declare(file, Stmts)
        file.write('\n')
        # Forward declarations complete

        declare_visitor(file, 'Visitor', Exprs, Stmts, 'Expr', 'Stmt')
        # Visitor declaration complete

        declare_base(file, 'Expr')
        declare_base(file, 'Stmt')

        declare_derived(file, 'Expr', Exprs[0],
            'target{target}, value{std::move(value)}',
            'Token target, expr_node_t<T> value')

        declare_derived(file, 'Expr', Exprs[1],
            'left{std::move(left)}, oper{oper}, right{std::move(right)}',
            'expr_node_t<T> left, Token oper, expr_node_t<T> right')

        declare_derived(file, 'Expr', Exprs[2],
            'function{std::move(function)}, paren{paren}, args{std::move(args)}',
            'expr_node_t<T> function, Token paren, std::vector<expr_node_t<T>> args')

        declare_derived(file, 'Expr', Exprs[3],
            'object{std::move(object)}, name{name}',
            'expr_node_t<T> object, Token name')
        
        declare_derived(file, 'Expr', Exprs[4],
            'expr{std::move(expr)}',
            'expr_node_t<T> expr')

        declare_derived(file, 'Expr', Exprs[5],
            'value{std::move(value)}',
            'T value')

        declare_derived(file, 'Expr', Exprs[6],
            'left{std::move(left)}, oper{oper}, right{std::move(right)}',
            'expr_node_t<T> left, Token oper, expr_node_t<T> right')

        declare_derived(file, 'Expr', Exprs[7],
            'object{std::move(object)}, name{name}, value{std::move(value)}',
            'expr_node_t<T> object, Token name, expr_node_t<T> value')

        declare_derived(file, 'Expr', Exprs[8],
            'keyword{keyword}, name{name}',
            'Token keyword, Token name')

        declare_derived(file, 'Expr', Exprs[9],
            'keyword{keyword}',
            'Token keyword')

        declare_derived(file, 'Expr', Exprs[10],
            'operator{operator}, right{std::move(right)}',
            'Token operator, expr_node_t<T> right')

        declare_derived(file, 'Expr', Exprs[11],
            'name{name}',
            'Token name')

        file.write('\n')
        end_file(file)
