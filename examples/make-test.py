def tab(file, n):
    file.write('    ' * n)
    return file

if __name__ == '__main__':
    a = lambda a: ord(a)
    b = lambda b: chr(b)
    with open('test.eis', 'wt') as f:
        for i in range(a('A'), a('Z') + 1):
            for j in range(a('A'), a('Z') + 1):
                c = str(b(i))
                d = str(b(j))
                f.write('class ' + c + d + ' {\n')
                tab(f, 1).write('public var x: int\n')
                tab(f, 1).write('private var y: string\n')
                tab(f, 1).write('public fn ' + c + d + '() -> const ' + c + d + ' {}\n')
                tab(f, 1).write('public fn foo(s: const ref ' + c + d + ') -> string { return "string"; }\n')
                tab(f, 1).write('private fn bar(s: const ref ' + c + d + ') -> null { var x = 0; print(x); }\n')
                f.write('}\n\n')
