### TODO

- [x] README.md
- [x] `ref` keyword, works like `var` and makes a reference to another variable
  (`ref x = y`)
- [ ] Lists
- [ ] `[1, foo(), bar(), ...]` (list expression)
- [ ] `0..foo(), 0..=foo()` (range expression)
- [ ] Tuples (`{int, string, float}`, `{0, "string", 5.0}`)
- [ ] Tuple unpacking (`tup..`)
- [ ] Classes
- [ ] Modules
- [ ] Documentation
- [ ] Move expressions
- [ ] First class functions
- [ ] Sum types, like in Rust/OCaml
- [ ] Lambda functions
- [ ] Lambda expressions
- [ ] Templates
- [ ] Standard library
- [ ] Unicode (most likely only UTF-8) support
- [ ] More integral types
- [ ] More floating types
- [ ] LLVM backend

### Bugs and issues

- [ ] Refactor `visit(UserDefinedType &)` in TypeResolver to return pointer to
  ClassStmt
- [x] Fix bug in accessing variables from outer scope (by implementing
  `Instruction::ACCESS_OUTER_SHORT`, `Instruction::ACCESS_OUTER_LONG`,
  `Instruction::MAKE_REF_TO_OUTER`)
- [x] Fix `Chunk::get_line_number` relying on instruction number (which is
  wrong), it should rely on the instruction pointer
- [x] Fix bug in parser where function defined inside class with same name as
  one outside causes an error
