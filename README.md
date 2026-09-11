# Simple Shell

A lightweight Unix-style command shell implemented in C for Linux, demonstrating process creation, command execution, pipelines, background jobs, built-ins, and command history.

## Features

- External command execution with `fork()` and `execvp()`
- Built-in `cd`, `history`, and `exit`
- Multi-stage pipelines using `pipe()` and `dup2()`
- Background execution with `&`
- Background process tracking with `waitpid(..., WNOHANG)`
- Command history with PID and execution duration
- Dynamic input handling with `getline()`

## Build and run

```bash
make
./simple-shell
```

Example:

```text
simple-shell>$$> ls
simple-shell>$$> cat file.txt | grep hello
simple-shell>$$> ./program &
simple-shell>$$> history
```

## OS concepts demonstrated

- Process creation and termination
- `exec` family and process replacement
- Inter-process communication with pipes
- File-descriptor redirection
- Foreground vs. background processes
- Parent-child synchronization

## Portfolio value

This project demonstrates practical Linux systems programming and understanding of the core mechanisms behind a command-line shell.

## Author

Dhruv Dagar
