# a4: Mini Version Control System

Spec: [assignment_4.pdf](assignment_4.pdf)

A small Git clone. Objects are zlib-compressed and addressed by SHA-1 under `.mygit`. Commands: `init`, `hash-object`, `cat-file`, `write-tree`, `ls-tree`, `add`, `commit`, `commit-tree`, `log`, `checkout`.

```sh
make build
./run.sh <command> [args]    # builds and runs the debug binary
```

Layout: `src/cli` argument handling, `src/commands` one file per command, `src/objects` blobs, trees and commits, `src/repo` and `src/core` shared code.
