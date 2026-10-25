# a1: File Reversal

Spec: [assignment_1.pdf](assignment_1.pdf)

- `q1.c`: reverses a file into `Assignment1/`. Modes: `0` block-wise, `1` full, `2` partial range.
- `q2.c`: checks permissions and that the output matches the chosen reversal.

```sh
make build
build/asan/q1 <file> 0 <block size>
build/asan/q1 <file> 1
build/asan/q1 <file> 2 <start> <end>
build/asan/q2 <new file> <old file> <dir> <flag> [<block size> | <start> <end>]
```
