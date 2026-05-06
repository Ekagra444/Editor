#include "search.h"
#include <string.h>

void find_next(EditorBuffer *buf) {
    if (strlen(search_query) == 0) return;

    int start_row = last_match_row;
    int start_col = last_match_col + 1;
    if (start_row < 0) {
        start_row = buf->cursor_row;
        start_col = buf->cursor_col;
    }

    // Search from current position to end of file
    for (int i = start_row; i < buf->line_count; i++) {
        char *line = buf->lines[i];
        char *pos  = (i == start_row)
            ? strstr(line + start_col, search_query)
            : strstr(line, search_query);
        if (pos) {
            last_match_row  = i;
            last_match_col  = pos - line;
            buf->cursor_row = i;
            buf->cursor_col = last_match_col;
            return;
        }
    }

    // Wrap: search from top up to where we started
    for (int i = 0; i <= start_row; i++) {
        char *pos = strstr(buf->lines[i], search_query);
        if (pos) {
            last_match_row  = i;
            last_match_col  = pos - buf->lines[i];
            buf->cursor_row = i;
            buf->cursor_col = last_match_col;
            return;
        }
    }
}
