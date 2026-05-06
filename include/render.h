#ifndef RENDER_H
#define RENDER_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "types.h"

void update_line_texture(SDL_Renderer *renderer, TTF_Font *font, EditorBuffer *buf, int line_index);
void render_sidebar(SDL_Renderer *renderer, TTF_Font *font);
void render_tabs(SDL_Renderer *renderer, TTF_Font *font);
void render_status_bar(SDL_Window *window, SDL_Renderer *renderer, TTF_Font *font, EditorBuffer *buf);
void render_command_bar(SDL_Renderer *renderer, TTF_Font *font);

#endif // RENDER_H
