# MUSH Interpreter
This project is an implemention of a simple scripting language interpreter for the MUSH programming language, a language roughly inspired by the BASIC language.

## Introduction
The interpreter manages concurrently executing lines of code from an input file. It is developed in C using concurrent processes, signal handling, and I/O redirection. This project is course work for a Programming in C course at Stony Brook University.

A MUSH program contains a set of statements with one statement per line. Statements either have required line numbers, no line numbers, or optional line numbers.

### Statements with no line numbers, so they are executed immediately: 

1. `list` - lists all statements with line numbers.
2. `delete` - deletes statements with line numbers within a specified range.
3. `run` - restarts the program counter and begins running from the lowest-numbered statement.
4. `cont` - continues execution from the statement which the program had stopped execution.

### Statements with optional line numbers, so they are executed immediately or in order:
1. `set` - sets the value of a variable to the result of evaluating an expression.
2. `unset` - unsets the value of a variable, leaving it with no value.
3. `goto` - resets the program counter so that the next statement to be executed is the one with the specified line number.
4. `if` - transfers control conditionally to the specified line number.

Statements can be a pipeline containing a sequence of commands that are executed in the foreground or in the background. A pipeline contains commands separated by vertical bars (`|`) and possibly input and/or output redirection (`<` and `>` respectively). To execute a pipeline, a group of processes is created to run the commands concurrently with the output of each command in the pipeline redirected to the next command. Input can be redirected from an input file. And output can be redirected to an output file. 

## Setup
This project is compiled and executed on the Linux Mint 20 Cinnamon operating system.

## Running the Project
Git clone this repository to your local computer running the Linux Mint 20 Cinnamon operating system. Compile the code using the `make` command, then run the MUSH interpreter and have it take in a sample input file with the following commands:
```
$ make clean all
# bin/mush < rsrc/loop1.mush
hello
hello
hello
^C
```

## Demo
