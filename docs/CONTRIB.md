# Contributing

## Code conventions

- Public APIs use the `pphp_` prefix. Value, array, string, and compiler
  internals use `pv_`, `pa_`, `ps_`, and `pc_` respectively.
- C99 is required. Variable-length arrays, `alloca`, and direct calls to
  `malloc`, `calloc`, `realloc`, or `free` are prohibited in runtime code.
- Include dependencies must be acyclic. Public declarations belong in
  `include/pphp`; implementation-only declarations belong in `src` or the
  relevant component directory.
- Prefer early returns and keep functions focused. A source file should stay
  below roughly 1,500 lines.
- Commits use Conventional Commit subjects such as `feat(parser): ...` and
  include tests for behavioral changes.

## Required checks

```sh
make test
make test-asan
make test-diff
make size
```

