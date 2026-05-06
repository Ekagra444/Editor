#ifndef TYPES_H
#define TYPES_H

#include <SDL.h>
#include <SDL_ttf.h>

// ---- Constants ----
#define MAX_HISTORY          100
#define MAX_LINES            100
#define MAX_COLS             256
#define CURSOR_BLINK_INTERVAL 700
#define MAX_FILES            256
#define MAX_TABS             10

// ---- Structs ----

typedef struct {
    int start_row, start_col;
    int end_row,   end_col;
    int active;
} Selection;

typedef struct {
    char lines[MAX_LINES][MAX_COLS];
    int  line_count;
    int  cursor_row;
    int  cursor_col;
} EditorState;

typedef struct {
    char lines[MAX_LINES][MAX_COLS];
    int  line_count;

    int  cursor_row;
    int  cursor_col;
    int  scroll_offset;

    char filename[256];

    int          dirty_flags[MAX_LINES];
    SDL_Texture *line_textures[MAX_LINES];

    EditorState undo_stack[MAX_HISTORY];
    int         undo_top;
    EditorState redo_stack[MAX_HISTORY];
    int         redo_top;

    int       modified;
    Selection selection;
} EditorBuffer;

typedef struct FileNode {
    char name[256];
    char path[512];
    int  is_dir;

    struct FileNode *children[128];
    int              child_count;
    int              expanded;
} FileNode;

typedef struct {
    FileNode *node;
    int       depth;
} VisibleNode;

// ---- Globals (defined in main.c, extern everywhere else) ----

extern EditorBuffer tabs[MAX_TABS];
extern int          tab_count;
extern int          active_tab;

extern VisibleNode visible[512];
extern int         visible_count;
extern int         selected_index;

extern FileNode *root;

extern int  search_mode;
extern char search_query[128];
extern int  search_len;
extern int  last_match_row;
extern int  last_match_col;

extern int  command_mode;
extern char command_buffer[256];
extern int  command_len;

#endif // TYPES_H
