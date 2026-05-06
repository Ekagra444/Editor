#include "editor.h"
#include <stdio.h>
#include <string.h>

void save_state(EditorBuffer *buf) {
    if (buf->undo_top >= MAX_HISTORY - 1) return;
    buf->undo_top++;
    EditorState *s = &buf->undo_stack[buf->undo_top];
    for (int i = 0; i < buf->line_count; i++)
        strcpy(s->lines[i], buf->lines[i]);
    s->line_count  = buf->line_count;
    s->cursor_row  = buf->cursor_row;
    s->cursor_col  = buf->cursor_col;
    buf->modified  = 0;
    buf->redo_top  = -1;
}

void restore_state(EditorBuffer *buf, EditorState *s) {
    buf->modified   = 1;
    buf->line_count = s->line_count;
    buf->cursor_row = s->cursor_row;
    buf->cursor_col = s->cursor_col;
    for (int i = 0; i < buf->line_count; i++) {
        strcpy(buf->lines[i], s->lines[i]);
        buf->dirty_flags[i] = 1;
    }
}

void normalize_selection(Selection *s) {
    if (s->start_row > s->end_row ||
       (s->start_row == s->end_row && s->start_col > s->end_col))
    {
        int tr = s->start_row, tc = s->start_col;
        s->start_row = s->end_row;
        s->start_col = s->end_col;
        s->end_row   = tr;
        s->end_col   = tc;
    }
}

void delete_selection(EditorBuffer *buf) {
    if (!buf->selection.active) return;
    save_state(buf);
    buf->modified = 1;
    normalize_selection(&buf->selection);

    int sr = buf->selection.start_row;
    int sc = buf->selection.start_col;
    int er = buf->selection.end_row;
    int ec = buf->selection.end_col;

    if (sr == er) {
        char *line = buf->lines[sr];
        memmove(&line[sc], &line[ec], strlen(line) - ec + 1);
        buf->cursor_row      = sr;
        buf->cursor_col      = sc;
        buf->dirty_flags[sr] = 1;
    } else {
        char *start_line = buf->lines[sr];
        char *end_line   = buf->lines[er];
        start_line[sc]   = '\0';
        strcat(start_line, &end_line[ec]);

        int shift = er - sr;
        for (int i = sr + 1; i < buf->line_count - shift; i++) {
            strcpy(buf->lines[i], buf->lines[i + shift]);
            buf->dirty_flags[i] = 1;
        }
        buf->line_count -= shift;
        buf->cursor_row  = sr;
        buf->cursor_col  = sc;
        buf->dirty_flags[sr] = 1;
    }
    buf->selection.active = 0;
}

void open_file_in_tab(const char *filename) {
    if (tab_count >= MAX_TABS) return;

    EditorBuffer *buf = &tabs[tab_count];
    memset(buf, 0, sizeof(EditorBuffer));
    strcpy(buf->filename, filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        strcpy(buf->lines[0], "");
        buf->line_count      = 1;
        buf->dirty_flags[0]  = 1;
        buf->line_textures[0] = NULL;
    } else {
        buf->line_count = 0;
        char buffer[MAX_COLS];
        while (fgets(buffer, MAX_COLS, fp) && buf->line_count < MAX_LINES) {
            buffer[strcspn(buffer, "\n")] = '\0';
            strcpy(buf->lines[buf->line_count], buffer);
            buf->dirty_flags[buf->line_count]    = 1;
            buf->line_textures[buf->line_count]  = NULL;
            buf->line_count++;
        }
        fclose(fp);
        if (buf->line_count == 0) {
            strcpy(buf->lines[0], "");
            buf->line_count = 1;
        }
    }

    buf->cursor_row        = 0;
    buf->cursor_col        = 0;
    buf->scroll_offset     = 0;
    buf->undo_top          = -1;
    buf->redo_top          = -1;
    buf->selection.active  = 0;
    buf->modified          = 0;

    active_tab = tab_count;
    tab_count++;
}

void save_file(EditorBuffer *buf) {
    buf->modified = 0;
    const char *filename = buf->filename;
    if (strlen(filename) == 0) filename = "output.txt";

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Could not save file: %s\n", filename);
        return;
    }
    for (int i = 0; i < buf->line_count; i++)
        fprintf(fp, "%s\n", buf->lines[i]);
    fclose(fp);
    printf("Saved: %s\n", filename);
}
