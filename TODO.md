### TODO

- [ ] Lists
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
- [ ] Refactor `TypeResolver::show_conversion_note`, `TypeResolver::show_equality_note`
- [x] Fix `TypeResolver::visit(GroupingExpr&)` to not return the *exact* type of the expression
  it contains, because that doesn't make sense for something like `(x)`
- [ ] Move utility functions of Parser into `ParserUtils.cpp` and those of TypeResolver into
  `TypeResolverUtils.cpp`
- [ ] Create stable C API to use the interpreter in other projects
- [ ] Make tests (many many of them)
- [ ] Make a command line interface
- [x] Merge `SharedData` into `BaseType`
- [ ] Remove switch case fallthrough (later enabled using something like annotations)
- [ ] Make error handling prettier (eg. printing a line above and below for context)
- [ ] Remove list size in list type
- [ ] Remove comma operator
- [ ] Fix clang warnings
- [ ] Make enum to replace `true`/`false` by enumerators to have better code, like when
calling `make_new_type` with the values for `is_const` and `is_ref`
- [x] Make `error()` accept a `std::vector` of `std::string(_view)?`s to reduce jankiness
in the code
- [x] Add `not` keyword as alternative to `!`
- [x] Change switch syntax from `case expr: stmt` to `expr -> stmt`
- [ ] Disallow `ListExpr`s being used directly in `==` and `!=` expressions
- [x] Fix if statements allowing use of list expressions as conditions
- [x] Replace all instances of `!` with `not`