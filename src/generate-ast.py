def make_header(file, guard):
    file.write('#pragma once\n\n')
    file.write('#ifndef ' + guard+ '\n')
    file.write('#  define ' + guard + '\n\n')

def end_file(file):
    file.write('#endif\n')

def forward_declare(file, names):
    for name in names:
        file.write('template <typename T>\n')
        file.write('struct ' + name + ';\n')

def declare_alias(file, alias, name):
    file.write('template <typename T>\n')
    file.write('using ' + alias + ' = std::unique_ptr<' + name +'<T>>;\n')

if __name__ == '__main__':
    with open('AST.hpp', 'wt') as file:
        make_header(file, 'AST_HPP')
        file.write('#include <memory>\n')
        file.write('#include "Token.hpp"\n\n')
        forward_declare(file, ['Expr', 'Stmt'])
        file.write('\n')
        declare_alias(file, 'expr_node_t', 'Expr')
        declare_alias(file, 'stmt_node_t', 'Stmt')
        file.write('\n')
        Exprs = ['Assign', 'Binary', 'Call', 'Get', 'Grouping', 'Literal', 'Logical', 'Set', 'Super', 'This', 'Unary', 'Variable']
        Stmt = ['Block', 'Class', 'Expression', 'Function', 'If', 'Print', 'Return', 'Val','Var', 'While']
        end_file(file)
