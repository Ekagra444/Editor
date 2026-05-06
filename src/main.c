#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "config.h"
#include "editor.h"
#include "render.h"
#include "sidebar.h"
#include "search.h"
#include "commands.h"

// ---- Global definitions (declared extern in types.h) ----

EditorBuffer tabs[MAX_TABS];
int          tab_count    = 0;
int          active_tab   = 0;

VisibleNode  visible[512];
int          visible_count = 0;
int          selected_index = 0;

FileNode    *root = NULL;

int          search_mode  = 0;
char         search_query[128] = "";
int          search_len   = 0;
int          last_match_row = -1;
int          last_match_col = -1;

int          command_mode = 0;
char         command_buffer[256] = "";
int          command_len  = 0;

// ---- Main ----

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    // Build the initial file tree
    const char *tree_root = (argc > 1) ? argv[1] : ".";
    root           = create_node(tree_root, tree_root, 1);
    root->expanded = 1;
    build_tree(root);

    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font *font = TTF_OpenFont(FONT_PATH, FONT_SIZE);
    if (!font) {
        printf("Font error: %s\n", TTF_GetError());
        return 1;
    }

    SDL_StartTextInput();

    int running = 1;
    SDL_Event event;

    while (running) {
        EditorBuffer *buf = &tabs[active_tab];

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running = 0; break; }

            // ---- Text input ----
            if (event.type == SDL_TEXTINPUT) {
                // Enter command mode when ':' is typed outside command mode
                if (!command_mode && strcmp(event.text.text, ":") == 0) {
                    command_mode = 1;
                    command_len  = 0;
                    command_buffer[0] = '\0';
                }

                if (buf->selection.active)
                    delete_selection(buf);

                if (command_mode) {
                    if (command_len < 255) {
                        strcat(command_buffer, event.text.text);
                        command_len++;
                    }
                    break;
                }

                if (search_mode) {
                    last_match_row = -1;
                    last_match_col = -1;
                    if (search_len < (int)sizeof(search_query) - 1) {
                        strcat(search_query, event.text.text);
                        search_len++;
                    }
                    break;
                }

                // Normal text insertion
                save_state(buf);
                buf->dirty_flags[buf->cursor_row] = 1;
                char *line = buf->lines[buf->cursor_row];
                int   len  = strlen(line);
                if (len < MAX_COLS - 1) {
                    memmove(&line[buf->cursor_col + 1],
                            &line[buf->cursor_col],
                            len - buf->cursor_col + 1);
                    line[buf->cursor_col] = event.text.text[0];
                    buf->cursor_col++;
                }
            }

            // ---- Mouse wheel ----
            if (event.type == SDL_MOUSEWHEEL) {
                if (event.wheel.y > 0 && buf->scroll_offset > 0)
                    buf->scroll_offset--;
                if (event.wheel.y < 0 && buf->scroll_offset < buf->line_count - 1)
                    buf->scroll_offset++;
            }

            // ---- Key events ----
            if (event.type == SDL_KEYDOWN) {
                int shift = event.key.keysym.mod & KMOD_SHIFT;
                int ctrl  = event.key.keysym.mod & KMOD_CTRL;
                int alt   = event.key.keysym.mod & KMOD_ALT;
                SDL_Keycode sym = event.key.keysym.sym;

                // Escape — exit any modal mode
                if (sym == SDLK_ESCAPE) {
                    search_mode  = 0;
                    command_mode = 0;
                    break;
                }

                // Ctrl+W — close active tab (bug fixed: was tabs[i-1])
                if (ctrl && sym == SDLK_w) {
                    if (tab_count <= 1) break;
                    for (int i = 0; i < MAX_LINES; i++) {
                        if (tabs[active_tab].line_textures[i])
                            SDL_DestroyTexture(tabs[active_tab].line_textures[i]);
                    }
                    for (int i = active_tab; i < tab_count - 1; i++)
                        tabs[i] = tabs[i + 1];
                    tab_count--;
                    if (active_tab >= tab_count) active_tab = tab_count - 1;
                    buf = &tabs[active_tab];
                    break;
                }

                // Ctrl+Tab — switch tabs
                if (ctrl && sym == SDLK_TAB) {
                    active_tab = shift
                        ? (active_tab - 1 + tab_count) % tab_count
                        : (active_tab + 1) % tab_count;
                    buf = &tabs[active_tab];
                    break;
                }

                // Ctrl+S — save
                if (ctrl && sym == SDLK_s) {
                    save_file(buf);
                    break;
                }

                // Ctrl+F — open search
                if (ctrl && sym == SDLK_f) {
                    search_mode    = 1;
                    search_len     = 0;
                    search_query[0] = '\0';
                    break;
                }

                // Ctrl+Z — undo
                if (ctrl && sym == SDLK_z) {
                    if (buf->undo_top >= 0) {
                        if (buf->redo_top < MAX_HISTORY - 1) {
                            buf->redo_top++;
                            EditorState *r = &buf->redo_stack[buf->redo_top];
                            for (int i = 0; i < buf->line_count; i++)
                                strcpy(r->lines[i], buf->lines[i]);
                            r->line_count = buf->line_count;
                            r->cursor_row = buf->cursor_row;
                            r->cursor_col = buf->cursor_col;
                        }
                        restore_state(buf, &buf->undo_stack[buf->undo_top]);
                        buf->undo_top--;
                    }
                    break;
                }

                // Ctrl+Y — redo
                if (ctrl && sym == SDLK_y) {
                    if (buf->redo_top >= 0) {
                        if (buf->undo_top < MAX_HISTORY - 1) {
                            buf->undo_top++;
                            EditorState *u = &buf->undo_stack[buf->undo_top];
                            for (int i = 0; i < buf->line_count; i++)
                                strcpy(u->lines[i], buf->lines[i]);
                            u->line_count = buf->line_count;
                            u->cursor_row = buf->cursor_row;
                            u->cursor_col = buf->cursor_col;
                        }
                        restore_state(buf, &buf->redo_stack[buf->redo_top]);
                        buf->redo_top--;
                    }
                    break;
                }

                // Ctrl+C — copy selection
                if (ctrl && sym == SDLK_c) {
                    Selection s = buf->selection;
                    normalize_selection(&s);
                    char clipboard[4096] = "";
                    for (int i = s.start_row; i <= s.end_row; i++) {
                        int start = (i == s.start_row) ? s.start_col : 0;
                        int end   = (i == s.end_row)   ? s.end_col
                                                        : strlen(buf->lines[i]);
                        strncat(clipboard, &buf->lines[i][start], end - start);
                        if (i != s.end_row) strcat(clipboard, "\n");
                    }
                    SDL_SetClipboardText(clipboard);
                    break;
                }

                // Ctrl+V — paste
                if (ctrl && sym == SDLK_v) {
                    save_state(buf);
                    char *clip = SDL_GetClipboardText();
                    if (!clip) break;
                    if (buf->selection.active) delete_selection(buf);

                    char *line    = buf->lines[buf->cursor_row];
                    char *newline = strchr(clip, '\n');

                    if (!newline) {
                        int len = strlen(line);
                        if (len + (int)strlen(clip) < MAX_COLS) {
                            memmove(&line[buf->cursor_col + strlen(clip)],
                                    &line[buf->cursor_col],
                                    len - buf->cursor_col + 1);
                            memcpy(&line[buf->cursor_col], clip, strlen(clip));
                            buf->cursor_col += strlen(clip);
                            buf->dirty_flags[buf->cursor_row] = 1;
                        }
                    } else {
                        char first[MAX_COLS], last_part[MAX_COLS], tail[MAX_COLS];
                        strncpy(first, clip, newline - clip);
                        first[newline - clip] = '\0';
                        strcpy(last_part, newline + 1);
                        strcpy(tail, &line[buf->cursor_col]);
                        line[buf->cursor_col] = '\0';
                        strcat(line, first);
                        buf->dirty_flags[buf->cursor_row] = 1;

                        for (int i = buf->line_count; i > buf->cursor_row + 1; i--)
                            strcpy(buf->lines[i], buf->lines[i - 1]);
                        strcpy(buf->lines[buf->cursor_row + 1], last_part);
                        strcat(buf->lines[buf->cursor_row + 1], tail);
                        buf->line_count++;
                        buf->cursor_row++;
                        buf->cursor_col = strlen(last_part);
                        buf->dirty_flags[buf->cursor_row] = 1;
                    }
                    SDL_free(clip);
                    break;
                }

                // Backspace
                if (sym == SDLK_BACKSPACE) {
                    if (command_mode) {
                        if (command_len > 1) command_buffer[--command_len] = '\0';
                        break;
                    }
                    if (search_mode) {
                        if (search_len > 0) search_query[--search_len] = '\0';
                        break;
                    }
                    if (buf->selection.active) { delete_selection(buf); break; }

                    save_state(buf);
                    buf->dirty_flags[buf->cursor_row] = 1;
                    if (buf->cursor_col > 0) {
                        char *line = buf->lines[buf->cursor_row];
                        memmove(&line[buf->cursor_col - 1],
                                &line[buf->cursor_col],
                                strlen(line) - buf->cursor_col + 1);
                        buf->cursor_col--;
                    } else if (buf->cursor_row > 0) {
                        int prev_len = strlen(buf->lines[buf->cursor_row - 1]);
                        buf->dirty_flags[buf->cursor_row - 1] = 1;
                        strcat(buf->lines[buf->cursor_row - 1],
                               buf->lines[buf->cursor_row]);
                        for (int i = buf->cursor_row; i < buf->line_count - 1; i++) {
                            buf->line_textures[i] = buf->line_textures[i + 1];
                            buf->dirty_flags[i]   = 1;
                            strcpy(buf->lines[i], buf->lines[i + 1]);
                        }
                        buf->line_textures[buf->line_count - 1] = NULL;
                        buf->line_count--;
                        buf->cursor_row--;
                        buf->cursor_col = prev_len;
                    }
                    break;
                }

                // Enter (guard against Alt — Alt+Enter is handled below for sidebar)
                if (sym == SDLK_RETURN && !alt) {
                    if (command_mode) {
                        execute_command(command_buffer);
                        command_mode = 0;
                        break;
                    }
                    if (search_mode) {
                        find_next(buf);
                        break;
                    }
                    if (buf->selection.active) delete_selection(buf);

                    save_state(buf);
                    buf->dirty_flags[buf->cursor_row]     = 1;
                    buf->dirty_flags[buf->cursor_row + 1] = 1;
                    char *line = buf->lines[buf->cursor_row];

                    for (int i = buf->line_count; i > buf->cursor_row + 1; i--) {
                        buf->line_textures[i] = buf->line_textures[i - 1];
                        strcpy(buf->lines[i], buf->lines[i - 1]);
                    }
                    buf->line_textures[buf->cursor_row + 1] = NULL;
                    strcpy(buf->lines[buf->cursor_row + 1], &line[buf->cursor_col]);
                    line[buf->cursor_col] = '\0';
                    buf->line_count++;
                    buf->cursor_row++;
                    buf->cursor_col = 0;
                    break;
                }

                // Alt+arrows — sidebar navigation
                if (alt && sym == SDLK_DOWN && selected_index < visible_count - 1) {
                    selected_index++;
                    break;
                }
                if (alt && sym == SDLK_UP && selected_index > 0) {
                    selected_index--;
                    break;
                }
                if (alt && sym == SDLK_RETURN) {
                    FileNode *node = visible[selected_index].node;
                    if (node->is_dir) {
                        node->expanded = !node->expanded;
                        if (node->expanded)
                            build_tree(node); // always refresh on expand
                    } else {
                        open_file_in_tab(node->path);
                    }
                    break;
                }

                // Arrow keys (with optional shift-select)
                if (sym == SDLK_LEFT) {
                    if (!shift) buf->selection.active = 0;
                    if (buf->cursor_col > 0) buf->cursor_col--;
                    if (shift) {
                        if (!buf->selection.active) {
                            buf->selection.start_row = buf->cursor_row;
                            buf->selection.start_col = buf->cursor_col + 1;
                            buf->selection.active    = 1;
                        }
                        buf->selection.end_row = buf->cursor_row;
                        buf->selection.end_col = buf->cursor_col;
                    }
                    break;
                }
                if (sym == SDLK_RIGHT) {
                    if (!shift) buf->selection.active = 0;
                    if (buf->cursor_col < (int)strlen(buf->lines[buf->cursor_row]))
                        buf->cursor_col++;
                    if (shift) {
                        if (!buf->selection.active) {
                            buf->selection.start_row = buf->cursor_row;
                            buf->selection.start_col = buf->cursor_col - 1;
                            buf->selection.active    = 1;
                        }
                        buf->selection.end_row = buf->cursor_row;
                        buf->selection.end_col = buf->cursor_col;
                    }
                    break;
                }
                if (sym == SDLK_UP) {
                    if (!shift) buf->selection.active = 0;
                    if (buf->cursor_row > 0) {
                        buf->cursor_row--;
                        buf->cursor_col = SDL_min(buf->cursor_col,
                            (int)strlen(buf->lines[buf->cursor_row]));
                    }
                    if (shift) {
                        if (!buf->selection.active) {
                            buf->selection.start_row = buf->cursor_row + 1;
                            buf->selection.start_col = buf->cursor_col;
                            buf->selection.active    = 1;
                        }
                        buf->selection.end_row = buf->cursor_row;
                        buf->selection.end_col = buf->cursor_col;
                    }
                    break;
                }
                if (sym == SDLK_DOWN) {
                    if (!shift) buf->selection.active = 0;
                    if (buf->cursor_row < buf->line_count - 1) {
                        buf->cursor_row++;
                        buf->cursor_col = SDL_min(buf->cursor_col,
                            (int)strlen(buf->lines[buf->cursor_row]));
                    }
                    if (shift) {
                        if (!buf->selection.active) {
                            buf->selection.start_row = buf->cursor_row - 1;
                            buf->selection.start_col = buf->cursor_col;
                            buf->selection.active    = 1;
                        }
                        buf->selection.end_row = buf->cursor_row;
                        buf->selection.end_col = buf->cursor_col;
                    }
                    break;
                }
            } // end SDL_KEYDOWN
        } // end event poll

        // Re-fetch buf after events (tab may have switched)
        buf = &tabs[active_tab];

        // ---- Scroll clamping ----
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        int line_height   = TTF_FontHeight(font);
        int bar_h         = line_height + STATUS_BAR_PADDING;
        int visible_lines = (win_h - bar_h - 50 - EDITOR_Y_OFFSET) / line_height;

        if (buf->cursor_row < buf->scroll_offset)
            buf->scroll_offset = buf->cursor_row;
        if (buf->cursor_row >= buf->scroll_offset + visible_lines)
            buf->scroll_offset = buf->cursor_row - visible_lines + 1;

        // ---- Rebuild visible sidebar nodes ----
        visible_count = 0;
        build_visible(root, 0);

        // ---- Clear ----
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        // ---- Search highlight (under text) ----
        if (search_mode && strlen(search_query) > 0) {
            for (int i = buf->scroll_offset;
                 i < buf->scroll_offset + visible_lines && i < buf->line_count; i++)
            {
                char *pos = strstr(buf->lines[i], search_query);
                if (!pos) continue;
                int col = pos - buf->lines[i];
                char temp[MAX_COLS];
                strncpy(temp, buf->lines[i], col);
                temp[col] = '\0';
                int offset, match_w;
                TTF_SizeText(font, temp, &offset, NULL);
                TTF_SizeText(font, search_query, &match_w, NULL);
                SDL_Rect rect = {
                    EDITOR_X + offset,
                    5 + EDITOR_Y_OFFSET + (i - buf->scroll_offset) * line_height,
                    match_w, line_height
                };
                SDL_SetRenderDrawColor(renderer, 120, 120, 40, 255);
                SDL_RenderFillRect(renderer, &rect);
            }
        }

        render_sidebar(renderer, font);

        // ---- Search bar or tab bar ----
        if (search_mode) {
            SDL_Rect bar = {EDITOR_X, 0, 400, TAB_BAR_HEIGHT};
            SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
            SDL_RenderFillRect(renderer, &bar);
            char display[160];
            sprintf(display, "Search: %s", search_query);
            SDL_Color   color = {255, 255, 255, 255};
            SDL_Surface *s    = TTF_RenderText_Blended(font, display, color);
            SDL_Texture *t    = SDL_CreateTextureFromSurface(renderer, s);
            SDL_Rect     dst  = {EDITOR_X + 10, 5, s->w, s->h};
            SDL_RenderCopy(renderer, t, NULL, &dst);
            SDL_FreeSurface(s);
            SDL_DestroyTexture(t);
        } else {
            render_tabs(renderer, font);
        }

        render_command_bar(renderer, font);

        // ---- Text lines ----
        for (int i = 0; i < visible_lines; i++) {
            int line_index = buf->scroll_offset + i;
            if (line_index >= buf->line_count) break;

            int y = EDITOR_Y_OFFSET + 10 + i * line_height;

            if (buf->dirty_flags[line_index]) {
                update_line_texture(renderer, font, buf, line_index);
                buf->dirty_flags[line_index] = 0;
            }

            // Selection highlight
            if (buf->selection.active) {
                Selection s = buf->selection;
                normalize_selection(&s);
                if (line_index >= s.start_row && line_index <= s.end_row) {
                    int start_x = EDITOR_X, end_x = EDITOR_X;
                    char temp[MAX_COLS];
                    if (line_index == s.start_row) {
                        strncpy(temp, buf->lines[line_index], s.start_col);
                        temp[s.start_col] = '\0';
                        int w; TTF_SizeText(font, temp, &w, NULL);
                        start_x += w;
                    }
                    if (line_index == s.end_row) {
                        strncpy(temp, buf->lines[line_index], s.end_col);
                        temp[s.end_col] = '\0';
                        int w; TTF_SizeText(font, temp, &w, NULL);
                        end_x += w;
                    } else {
                        int w; TTF_SizeText(font, buf->lines[line_index], &w, NULL);
                        end_x += w;
                    }
                    SDL_Rect rect = {start_x, y, end_x - start_x, line_height};
                    SDL_SetRenderDrawColor(renderer, 60, 60, 120, 255);
                    SDL_RenderFillRect(renderer, &rect);
                }
            }

            // Render cached texture
            if (buf->line_textures[line_index]) {
                int w, h;
                SDL_QueryTexture(buf->line_textures[line_index], NULL, NULL, &w, &h);
                SDL_Rect dst = {EDITOR_X, y, w, h};
                SDL_RenderCopy(renderer, buf->line_textures[line_index], NULL, &dst);
            }
        }

        render_status_bar(window, renderer, font, buf);

        // ---- Blinking cursor ----
        char temp[MAX_COLS];
        strncpy(temp, buf->lines[buf->cursor_row], buf->cursor_col);
        temp[buf->cursor_col] = '\0';
        int cursor_offset = 0;
        TTF_SizeText(font, temp, &cursor_offset, NULL);

        int    cursor_x   = EDITOR_X + cursor_offset;
        int    cursor_y   = EDITOR_Y_OFFSET + 10 +
                            (buf->cursor_row - buf->scroll_offset) * line_height;
        Uint32 now        = SDL_GetTicks();
        if ((now / CURSOR_BLINK_INTERVAL) % 2 == 0) {
            SDL_Rect cursor_rect = {cursor_x, cursor_y, 2, line_height};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &cursor_rect);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(FRAME_DELAY_MS);
    } // end main loop

    // ---- Cleanup ----
    for (int t = 0; t < tab_count; t++)
        for (int i = 0; i < MAX_LINES; i++)
            if (tabs[t].line_textures[i])
                SDL_DestroyTexture(tabs[t].line_textures[i]);

    free_tree(root);
    SDL_StopTextInput();
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
