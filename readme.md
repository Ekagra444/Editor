# Ekagra Editor

A lightweight SDL2-based code editor with syntax highlighting, tabs, and an integrated file tree.

## Building

```bash
make
```

Requires SDL2 and SDL2_ttf. On macOS with Homebrew:

```bash
brew install sdl2 sdl2_ttf
```

## Running

```bash
./ekagra [directory]   # opens a directory in the file tree (defaults to .)
```

## Keybindings

| Key           | Action                          |
|---------------|---------------------------------|
| Ctrl+S        | Save current file               |
| Ctrl+Z        | Undo                            |
| Ctrl+Y        | Redo                            |
| Ctrl+C        | Copy selection                  |
| Ctrl+V        | Paste                           |
| Ctrl+F        | Open search bar                 |
| Ctrl+Tab      | Next tab                        |
| Ctrl+Shift+Tab| Previous tab                    |
| Ctrl+W        | Close current tab               |
| Alt+↑/↓       | Navigate file tree              |
| Alt+Enter     | Open file / expand folder       |
| Shift+Arrows  | Select text                     |
| Escape        | Exit search / command mode      |

## Commands

Type `:` to enter command mode, then:

| Command         | Action                         |
|-----------------|--------------------------------|
| `:new <file>`   | Create and open a new file     |
| `:del <file>`   | Delete a file                  |
| `:ren <old> <new>` | Rename a file              |
| `:open <file>`  | Open a file in a new tab       |

## Project Structure

```
ekagra/
├── include/
│   ├── types.h      # Shared structs, constants, extern globals
│   ├── config.h     # Layout constants, font path, window size
│   ├── editor.h
│   ├── render.h
│   ├── sidebar.h
│   ├── search.h
│   └── commands.h
├── src/
│   ├── main.c       # SDL init, global definitions, event loop
│   ├── editor.c     # Cursor, insert, delete, undo/redo, file I/O
│   ├── render.c     # SDL draw calls, syntax-highlight textures
│   ├── sidebar.c    # File tree (FileNode), build/free/traverse
│   ├── search.c     # find_next with wrap-around
│   └── commands.c   # :new :del :ren :open
└── Makefile
```
