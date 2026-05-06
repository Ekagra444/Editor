#include "render.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// ---- Internal helpers ----

static int is_keyword(const char *word) {
    for (int i = 0; i < KEYWORD_COUNT; i++)
        if (strcmp(word, KEYWORDS[i]) == 0) return 1;
    return 0;
}

// ---- Public API ----

void update_line_texture(SDL_Renderer *renderer, TTF_Font *font,
                         EditorBuffer *buf, int idx)
{
    if (buf->line_textures[idx]) {
        SDL_DestroyTexture(buf->line_textures[idx]);
        buf->line_textures[idx] = NULL;
    }

    const char *line = buf->lines[idx];
    int total_width  = 0;
    int height       = TTF_FontHeight(font);
    int i            = 0;

    // First pass: measure total width
    while (line[i]) {
        char temp[128];
        int  j = 0;
        if (line[i] == '/' && line[i + 1] == '/') {
            int w;
            TTF_SizeText(font, &line[i], &w, NULL);
            total_width += w;
            break;
        }
        if (isalpha(line[i]))      { while (isalnum(line[i])) temp[j++] = line[i++]; }
        else if (isdigit(line[i])) { while (isdigit(line[i])) temp[j++] = line[i++]; }
        else                       { temp[j++] = line[i++]; }
        temp[j] = '\0';
        int w;
        TTF_SizeText(font, temp, &w, NULL);
        total_width += w;
    }
    if (total_width == 0) total_width = 1;

    SDL_Surface *surface = SDL_CreateRGBSurface(
        0, total_width, height, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surface) return;
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

    // Second pass: blit colored token surfaces
    int offset_x = 0;
    i = 0;
    while (line[i]) {
        char      temp[128];
        int       j     = 0;
        SDL_Color color = {255, 255, 255, 255};

        if (line[i] == '/' && line[i + 1] == '/') {
            strcpy(temp, &line[i]);
            color = (SDL_Color){100, 200, 100, 255};
            i    += strlen(&line[i]);
        } else if (isalpha(line[i])) {
            while (isalnum(line[i])) temp[j++] = line[i++];
            temp[j] = '\0';
            if (is_keyword(temp))
                color = (SDL_Color){80, 160, 255, 255};
        } else if (isdigit(line[i])) {
            while (isdigit(line[i])) temp[j++] = line[i++];
            temp[j] = '\0';
            color   = (SDL_Color){255, 200, 100, 255};
        } else {
            temp[j++] = line[i++];
            temp[j]   = '\0';
        }

        SDL_Surface *token = TTF_RenderText_Blended(font, temp, color);
        if (!token) continue;
        SDL_Rect dst = {offset_x, 0, token->w, token->h};
        SDL_BlitSurface(token, NULL, surface, &dst);
        offset_x += token->w;
        SDL_FreeSurface(token);
    }

    buf->line_textures[idx] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
}

void render_sidebar(SDL_Renderer *renderer, TTF_Font *font) {
    int line_height = TTF_FontHeight(font);

    SDL_Rect bg = {0, 0, SIDEBAR_WIDTH, 600};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(renderer, &bg);

    for (int i = 0; i < visible_count; i++) {
        int y = 10 + i * line_height;
        SDL_Color color = {200, 200, 200, 255};

        if (i == selected_index) {
            SDL_SetRenderDrawColor(renderer, 80, 80, 120, 255);
            SDL_Rect sel = {0, y, SIDEBAR_WIDTH, line_height};
            SDL_RenderFillRect(renderer, &sel);
            color = (SDL_Color){255, 255, 255, 255};
        }

        const char *path = visible[i].node->path;
        const char *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;

        char display[300];
        if (visible[i].node->is_dir)
            sprintf(display, "%s %s", visible[i].node->expanded ? "[-]" : "[+]", name);
        else
            sprintf(display, "    %s", visible[i].node->name);

        SDL_Surface *s = TTF_RenderText_Blended(font, display, color);
        SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
        int indent     = visible[i].depth * 15;
        SDL_Rect dst   = {10 + indent, y, s->w, s->h};
        SDL_RenderCopy(renderer, t, NULL, &dst);
        SDL_FreeSurface(s);
        SDL_DestroyTexture(t);
    }
}

void render_tabs(SDL_Renderer *renderer, TTF_Font *font) {
    int current_x = EDITOR_X;
    SDL_Color white = {255, 255, 255, 255};

    for (int i = 0; i < tab_count; i++) {
        const char *label = tabs[i].filename;
        const char *slash = strrchr(label, '/');
        if (slash) label = slash + 1;

        char display[20];
        strncpy(display, label, 19);
        display[19] = '\0';

        SDL_Surface *s = TTF_RenderText_Blended(font, display, white);
        if (!s) continue;

        int      tab_width = s->w + TAB_PADDING;
        SDL_Rect rect      = {current_x, 0, tab_width, TAB_BAR_HEIGHT};

        SDL_SetRenderDrawColor(renderer,
            i == active_tab ? 80 : 50,
            i == active_tab ? 80 : 50,
            i == active_tab ? 120 : 50,
            255);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(renderer, &rect);

        SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
        SDL_Rect     dst = {rect.x + TAB_PADDING / 2,
                            (TAB_BAR_HEIGHT - s->h) / 2,
                            s->w, s->h};
        SDL_RenderCopy(renderer, t, NULL, &dst);
        current_x += tab_width;

        SDL_FreeSurface(s);
        SDL_DestroyTexture(t);
    }
}

void render_status_bar(SDL_Window *window, SDL_Renderer *renderer,
                       TTF_Font *font, EditorBuffer *buf)
{
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);
    int bar_h = TTF_FontHeight(font) + STATUS_BAR_PADDING;

    SDL_Rect bar = {0, win_h - bar_h, win_w, bar_h};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderFillRect(renderer, &bar);

    char status[512];
    sprintf(status, "%s%s | Ln %d, Col %d | Lines: %d | %s",
        buf->filename,
        buf->modified ? " *" : "",
        buf->cursor_row + 1,
        buf->cursor_col + 1,
        buf->line_count,
        search_mode ? "SEARCH" : "NORMAL");

    SDL_Color  color = {200, 200, 200, 255};
    SDL_Surface *s   = TTF_RenderText_Blended(font, status, color);
    SDL_Texture *t   = SDL_CreateTextureFromSurface(renderer, s);
    SDL_Rect     dst = {10, win_h - bar_h + 3, s->w, s->h};
    SDL_RenderCopy(renderer, t, NULL, &dst);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

void render_command_bar(SDL_Renderer *renderer, TTF_Font *font) {
    if (!command_mode) return;

    SDL_Rect bar = {EDITOR_X, 0, 600, TAB_BAR_HEIGHT};
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderFillRect(renderer, &bar);

    SDL_Color   color = {255, 255, 255, 255};
    SDL_Surface *s    = TTF_RenderText_Blended(font, command_buffer, color);
    SDL_Texture *t    = SDL_CreateTextureFromSurface(renderer, s);
    SDL_Rect     dst  = {EDITOR_X + 10, 5, s->w, s->h};
    SDL_RenderCopy(renderer, t, NULL, &dst);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}
