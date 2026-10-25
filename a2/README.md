# a2: POSIX Shell

Spec: [assignment_2.pdf](assignment_2.pdf)

An interactive shell with `;`-separated commands, background jobs (`&`), pipes, `<` `>` `>>` redirection, signals, tab autocomplete and history. Built-ins: `cd`, `echo`, `pwd`, `ls`, `pinfo`, `search`, `history`, `jobs`, `type`.

```sh
make build
./run.sh    # builds and runs the debug binary
```

Layout: `src/` shell core, `src/builtins/` built-ins, `src/ls/` `ls`.
