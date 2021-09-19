### TODO

- [x] Lists
- [x] `0..foo(), 0..=foo()` (range expression)
- [ ] Tuples (`{int, string, float}`, `{0, "string", 5.0}`)
- [ ] Tuple unpacking (`tup..`)
- [ ] Export AST as JSON
- [ ] Classes
- [ ] Modules
- [ ] Documentation
- [x] Move expressions
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
- [ ] Move utility functions of Parser into `ParserUtils.cpp` and those of TypeResolver into
  `TypeResolverUtils.cpp`
- [ ] Create stable C API to use the interpreter in other projects
- [ ] Make tests (many many of them)
- [ ] Make a command line interface
- [ ] Remove switch case fallthrough (later enabled using something like annotations)
- [ ] Make error handling prettier (eg. printing a line above and below for context)
- [ ] Remove list size in list type
- [ ] Remove comma operator
- [ ] Fix clang warnings
- [ ] Make enum to replace `true`/`false` by enumerators to have better code, like when
calling `make_new_type` with the values for `is_const` and `is_ref`
- [ ] Disallow `ListExpr`s being used directly in `==` and `!=` expressions
- [ ] Fix the order of type checking in `TypeResolver`, check functions before classes,
before everything else
- [x] Fix dumb unused variable warning
- [ ] Move some `TypeResolver` functions into `AST.hpp`
- [x] Reorder project, moving header files into `include/`, separating interpreter into
`Frontend/` and `Backend/` directories
- [ ] Rewrite the Scanner
- [ ] Redo Native functions interfaces
- [ ] Add pair syntax `x: y`, same as `{x, y}`
- [x] Rename `resolved` and `ExprTypeInfo` to `synthesized_attrs` and `ExprSynthesizedAttrs`
- [ ] Possibly add inherited attributes with `inherited_attrs` members and
`ExprInheritedAttrs` data type
- [x] Fix tuple assignment in `SetExpr` having no type checking
- [ ] Implement references to class member variables at runtime
- [x] Replace `... == Type::LIST || ... == Type::TUPLE || ... == Type::CLASS` in
`CodeGen` with `is_nontrivial_type` call
- [x] Rename `builtin`/`inbuilt` to `trivial`
- [ ] Move expressions for list indices, tuple members and class member variables
- [ ] Destructor calls, including those of member variables
- [ ] Add `member_map` and `method_map` in `ClassStmt` for `string_view` -> `size_t`
mapping from member/method name to index in `members`/`methods` vectors
