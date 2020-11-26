def tab(file, n):
    file.write('    ' * n)
    return file

if __name__ == '__main__':
    a = lambda a: ord(a)
    b = lambda b: chr(b)
    with open('test.wis', 'wt') as f:
        for i in range(a('A'), a('I') + 1):
            for j in range(a('A'), a('I') + 1):
                for k in range(a('A'), a('I') + 1):
                    class_name = b(i) + b(j) + b(k)
                    f.write('class ' + class_name + ' {\n')
                    for l in range(a('a'), a('z') + 1):
                        for k in range(a('a'), a('z') + 1):
                            var_name = b(l) + b(k)
                            if var_name != 'fn' and var_name != 'or' and var_name != 'if':
                                tab(f, 1).write('public var ' + var_name + ': int = 0\n')
                    tab(f, 1).write('public fn ' + class_name + '() -> const ' + class_name + ' {}\n')
                    tab(f, 1).write('public fn foo(s: const ref ' + class_name + ') -> string { return "string"; }\n')
                    tab(f, 1).write('private fn bar(s: const ref ' + class_name + ') -> null { var x = 0; print(x); }\n')
                    f.write('}\n\n')
