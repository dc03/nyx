## wis

wis is a simple, interpreted language written in C++.

```c++
fn main() -> int {
    print("Hello world!")
    return 0
}
```

A reference of the language grammar is available in the file `grammar.bnf`.

### Features

- Strictly, statically-typed type system
- Completely deterministic object lifetimes, enabling the use of destructors
  (like in C++)
- No Garbage Collector
- Pseudo - UFCS (Uniform Function Call Syntax), where methods are not bound to 
  classes, and are instead free functions scoped to the name of the
  class
- Optional semicolons, where a newline can be used as a statement terminator
  instead of a semicolon
- Copy, reference and (hopefully) move semantics

### Building

Requires at least C++17.
```shell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=TYPE ../
make
```
where `TYPE` is one of `Debug` or `Release`.

<u>Note</u>: The release build enables LTO, so if `clang` is being used, linkers
like `gold`, `lld` which can compile clang's ThinLTO output are needed.

If a build system other than `make` is preferred, it can additionally be passed
to `cmake` using the `-G` flag, for example `cmake -G Ninja ...`.
