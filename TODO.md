### TODO

- [x] README.md
- [x] `ref` keyword, works like `var` and makes a reference to another variable
  (`ref x = y`)
- [ ] Lists
- [ ] `[1, foo(), bar(), ...]` (list expression)
- [ ] `0..foo(), 0..=foo()` (range expression)
- [ ] Tuples (`{int, string, float}`, `{0, "string", 5.0}`)
- [ ] Tuple unpacking (`tup..`)
- [ ] Export AST as JSON
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
- [ ] Type statements, `type x = y`

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
- [x] Fix bug with control flow flowing off the end of a function leading to
  interpreter halting
- [x] Fix bug with Parser segfaulting when `infix` is `nullptr`, like in `i++`
- [ ] Refactor `TypeResolver::show_conversion_note`, `TypeResolver::show_equality_note`
- [x] Refactor `Parser` interface
- [x] Refactor `VM` interface
- [ ] Fix `TypeResolver::visit(GroupingExpr&)` to not return the *exact* type of the expression
  it contains, because that doesn't make sense for something like `(x)`
- [x] ~~Move `enum class Type` from `VisitorTypes.hpp` to `AST.hpp` where it truly belongs~~
- [x] Move `VisitorTypes.{hpp,cpp}` into top level `src/` directory
- [x] Fix references not being dereferenced in CodeGen when they should be
- [ ] Move utility functions of Parser into `ParserUtils.cpp` and those of TypeResolver into
  `TypeResolverUtils.cpp`
- [x] Make `RequiresCopy` as alias to `bool` in AST and update `std::get` to use it instead of
numeric `2`
- [x] Update `std::get` to use `NumericConversionType` instead of numeric `1`, same thing for `0`
- [ ] Create stable C API to use the interpreter in other projects
- [ ] Make tests (many many of them)
- [x] Clean up examples
- [ ] Make a command line interface
- [ ] Merge `SharedData` into `BaseType`
- [x] Enforce curly braces after `if`, `for`, `while`, `switch` etc.
- [x] Replace `val` keyword by `const`
- [ ] Remove switch case fallthrough (later enabled using something like annotations)
- [ ] Make error handling prettier (eg. printing a line above and below for context)
- [ ] Remove list size in list type