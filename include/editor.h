#ifndef EDITOR_H
#define EDITOR_H

#include "types.h"

void save_state(EditorBuffer *buf);
void restore_state(EditorBuffer *buf, EditorState *s);
void normalize_selection(Selection *s);
void delete_selection(EditorBuffer *buf);
void open_file_in_tab(const char *filename);
void save_file(EditorBuffer *buf);

#endif // EDITOR_H
