# minishell

A small Unix shell in C, with the command line parsed by a lex scanner and a
yacc grammar rather than by hand-rolled string splitting.

## What it supports

```sh
ls -l | grep '^d' | wc -l        # pipelines of arbitrary length
sort < input.txt > output.txt    # stdin and stdout redirection
make >& build.log                # stderr redirection
sleep 30 &                       # background execution
echo $HOME/$USER                 # environment variable expansion
ls fil?.txt                      # single-character wildcard
cd /tmp                          # builtin
umask 022                        # builtin
```

## How it works

**Parsing.** `scanner.l` tokenises the line into words and the four operators
(`|`, `<`, `>`, `&`); `parser.y` holds the grammar. Every reduction calls back
into the shell — `command()` appends an argument, `pipeline()` closes the
current command and starts the next — so the parser fills the argument vectors
directly instead of building a tree that would then have to be walked.

Redirections are collected into a three-slot `filev` array, one per stream, and
the grammar rejects a second redirection of the same stream at parse time rather
than at execution time.

**Execution.** The shell forks one child per pipeline stage and creates a pipe
between each pair. Each child rewires its ends with `dup2`, closes every
descriptor it does not need — the part that is easy to get wrong, because a
single leaked write end leaves the next stage blocked on a read that never
returns — and calls `execvp`.

For a foreground pipeline the shell waits for the children; for a background one
it does not, and prints the process group so the job can be found later.

**Expansion.** Variable and wildcard expansion happen after parsing and before
execution, so a `$VAR` holding spaces expands to one argument rather than being
re-tokenised.

**Signals.** `SIGINT` and `SIGQUIT` are ignored in the shell itself and restored
to their defaults in the children, so Ctrl-C kills the running command and
leaves the prompt alive.

## Build and run

Requires `gcc`, `flex` and `bison` (or `lex` and `yacc`).

```sh
make
./msh
```

## Layout

| File | Contents |
|---|---|
| `main.c` | Execution: fork, pipes, redirection, expansion, builtins |
| `scanner.l` | Lexer — words and operators |
| `parser.y` | Grammar and the callbacks that fill the argument vectors |
| `Makefile` | Provided with the assignment; not to be modified |

## Licence

See `LICENCIA` — Licencia de Proyecto Educativo Práctico (LPEP). This was an
assignment at ETSIINF, Universidad Politécnica de Madrid, built on a skeleton
authored by the course staff.
