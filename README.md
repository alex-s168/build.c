# Build.C
Simple build "system".

Example build scripts using this system:
- [example 1](example.c)
- [project: Kollektions](https://github.com/alex-s168/kollektions/blob/master/build.c)
- [project: Sequencia](https://github.com/alex-s168/sequencia/blob/main/build.c)

## Getting started
```bash
# Add build.c as git submodule (you can also use other methods to get it)
$ git submodule add https://github.com/alex-s168/build.c.git build_c

# Copy example build script 
$ cp build_c/example.c build.c

# Modify build script now

# Build build script
$ cc build.c -o build.exe

# List all targets
$ ./build.exe 

# Build specific target
$ ./build.exe [target]
```
