# Gegtoo

A lightweight and minimal version control system implemented in C.  
Built primarily as an academic project to understand how Git works.

## Requirements

- C compiler (GCC or Clang)
- CMake (v3.10+)
- Make
- POSIX system (Linux/macOS)

## Structure

- `src/` — source files  
  - `commands/` — CLI commands  
  - `utils/` — SHA-1, Huffman  
- `include/` — headers  
- `driver/` — test running files  
- `resources/` — references  

## Build

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

After building, the `geg` executable will be located in your `build/` directory (or in the project root, depending on your CMake setup). The location of the `.geg` repository directory will also depend on the CMake configuration.

### Example Commands & Options

```bash
./geg init
./geg init <repo_name>

./geg status

./geg add .
./geg add <file_path> <folder_path>

./geg commit -m "Your commit message"

./geg log

./geg cat <hash>

./geg checkout <commit_hash>
./geg checkout <branch_name>

./geg remove <file_path>
./geg remove <file_path1> <file_path2>
./geg remove --cached <file_path>
```
