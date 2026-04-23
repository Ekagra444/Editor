#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include<dirent.h>

#define MAX_HISTORY 100
#define MAX_LINES 100
#define MAX_COLS  256
#define CURSOR_BLINK_INTERVAL 700
#define MAX_FILES 256
#define MAX_TABS 10

typedef struct{
	int start_row,start_col;
	int end_row,end_col;
	int active;
}Selection;

typedef struct{
	char lines[MAX_LINES][MAX_COLS];
	int line_count;
	int cursor_row;
	int cursor_col;
}EditorState;

typedef struct {
	char lines[MAX_LINES][MAX_COLS];
	int line_count;

	int cursor_row;
	int cursor_col;

	int scroll_offset;

	char filename[256];

	int dirty_flags[MAX_LINES];
	SDL_Texture* line_textures[MAX_LINES];

	// per-tab undo/redo
	EditorState undo_stack[MAX_HISTORY];
	int undo_top;
	EditorState redo_stack[MAX_HISTORY];
	int redo_top;

	Selection selection;
} EditorBuffer;

EditorBuffer tabs[MAX_TABS];
int tab_count = 0;
int active_tab = 0;

char files[MAX_FILES][256];
int file_count=0;
int selected_file=0;
int editor_x=260;
int editor_y_offset=40;

int search_mode=0;
char search_query[128]="";
int search_len=0;

int match_row=-1;
int match_col=-1;

const char *keywords[] = {
    "int", "return", "if", "else", "while", "for", "void", "char", "float", "double"
};
int keyword_count = sizeof(keywords) / sizeof(keywords[0]);

// --- Functions -----
void load_directory(const char *path){
	DIR *dir = opendir(path);
	if(!dir){
		printf("couldn't open directory\n");
		return;
	}
	struct dirent *entry;
	file_count=0;
	while((entry=readdir(dir))!=NULL&&file_count<MAX_FILES){
		if(strcmp(entry->d_name,".")==0||strcmp(entry->d_name,"..")==0)continue;
		strcpy(files[file_count],entry->d_name);
		file_count++;
	}
	closedir(dir);
}

void render_sidebar(SDL_Renderer *renderer, TTF_Font *font){
	int sidebar_width=250;
	int line_height = TTF_FontHeight(font);

	SDL_Rect bg = {0,0,sidebar_width,600};
	SDL_SetRenderDrawColor(renderer,40,40,40,255);
	SDL_RenderFillRect(renderer,&bg);

	for(int i=0;i<file_count;i++){
		int y = 10 + i*line_height;
		SDL_Color color = {200,200,200,255};
		if(i==selected_file){
			SDL_SetRenderDrawColor(renderer,80,80,120,255);
			SDL_Rect sel = {0,y,sidebar_width,line_height};
			SDL_RenderFillRect(renderer,&sel);
			color=(SDL_Color){255,255,255,255};
		}
		SDL_Surface *s = TTF_RenderText_Blended(font,files[i],color);
		SDL_Texture *t=SDL_CreateTextureFromSurface(renderer,s);
		SDL_Rect dst={10,y,s->w,s->h};
		SDL_RenderCopy(renderer,t,NULL,&dst);
		SDL_FreeSurface(s);
		SDL_DestroyTexture(t);
	}
}

void render_tabs(SDL_Renderer *renderer, TTF_Font *font) {
    int current_x = 260; // Starting x position
    int height = 30;
    int padding = 20;    // Extra space around the text

    for (int i = 0; i < tab_count; i++) {
        SDL_Color color = {255, 255, 255, 255};
        
        // 1. Prepare the label
        const char *label = tabs[i].filename;
        const char *slash = strrchr(label, '/');
        if (slash) label = slash + 1;

        char display[20];
        strncpy(display, label, 19);
        display[19] = '\0';

        // 2. Create surface to determine text dimensions
        SDL_Surface *s = TTF_RenderText_Blended(font, display, color);
        if (!s) continue;

        int tab_width = s->w + padding; 
        SDL_Rect rect = {current_x, 0, tab_width, height};

        // 3. Draw Tab Background
        if (i == active_tab)
            SDL_SetRenderDrawColor(renderer, 80, 80, 120, 255);
        else
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);

        SDL_RenderFillRect(renderer, &rect);

        // 4. Draw Border
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(renderer, &rect);

        // 5. Render Texture
        SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
        SDL_Rect dst = {rect.x + (padding / 2), (height - s->h) / 2, s->w, s->h};
        SDL_RenderCopy(renderer, t, NULL, &dst);

        // 6. Cleanup and increment X for next tab
        current_x += tab_width; 

        SDL_FreeSurface(s);
        SDL_DestroyTexture(t);
    }
}
int is_keyword(const char *word){
	for(int i=0;i<keyword_count;i++){
		if(strcmp(word,keywords[i])==0)return 1;
	}
	return 0;
}

void normalize_selection(Selection*);

void save_state(EditorBuffer *buf){
	if(buf->undo_top>=MAX_HISTORY-1)return;
	buf->undo_top++;
	EditorState *s= &buf->undo_stack[buf->undo_top];
	for(int i=0;i<buf->line_count;i++){
		strcpy(s->lines[i],buf->lines[i]);
	}
	s->line_count=buf->line_count;
	s->cursor_row=buf->cursor_row;
	s->cursor_col=buf->cursor_col;

	buf->redo_top=-1;
}

void delete_selection(EditorBuffer *buf){
	if(!buf->selection.active)return;
	save_state(buf);
	normalize_selection(&buf->selection);
	int sr = buf->selection.start_row;
	int sc = buf->selection.start_col;
	int er = buf->selection.end_row;
	int ec = buf->selection.end_col;

	if(sr==er){
		char *line=buf->lines[sr];
		memmove(&line[sc],&line[ec],strlen(line)-ec+1);
		buf->cursor_row=sr;
		buf->cursor_col=sc;
		buf->dirty_flags[sr]=1;
	}
	else{
		char *start_line = buf->lines[sr];
		char *end_line = buf->lines[er];
		start_line[sc]='\0';
		strcat(start_line,&end_line[ec]);
		int shift = er-sc;
		for(int i=sr+1;i<buf->line_count-shift;i++){
			strcpy(buf->lines[i],buf->lines[i+shift]);
			buf->dirty_flags[i]=1;
		}
		buf->line_count-=shift;
		buf->cursor_row=sr;
		buf->cursor_col=sc;
		buf->dirty_flags[sr]=1;
	}
	buf->selection.active=0;
}

void restore_state(EditorBuffer *buf, EditorState *s){
	buf->line_count = s->line_count;
	buf->cursor_row=s->cursor_row;
	buf->cursor_col=s->cursor_col;

	for(int i=0;i<buf->line_count;i++){
		strcpy(buf->lines[i],s->lines[i]);
		buf->dirty_flags[i]=1;
	}
}

void normalize_selection(Selection *s){
	if(s->start_row>s->end_row || (s->start_row == s->end_row && s->start_col>s->end_col)){
	int tr=s->start_row,tc=s->start_col;
	s->start_row=s->end_row;
	s->start_col=s->end_col;
	s->end_row=tr;
	s->end_col=tc;
	}
}

void update_line_texture(SDL_Renderer *renderer, TTF_Font *font, EditorBuffer *buf, int i)
{
    if (buf->line_textures[i]) {
        SDL_DestroyTexture(buf->line_textures[i]);
        buf->line_textures[i] = NULL;
    }

    const char *line = buf->lines[i];

    int total_width = 0;
    int height = TTF_FontHeight(font);
    int idx = 0;
    while (line[idx]) {
        char temp[128]; int j = 0;
        if (line[idx] == '/' && line[idx+1] == '/') {
            strcpy(temp, &line[idx]);
            int w; TTF_SizeText(font, temp, &w, NULL);
            total_width += w;
            break;
        }
        if (isalpha(line[idx])) {
            while (isalnum(line[idx])) temp[j++] = line[idx++];
        }
        else if (isdigit(line[idx])) {
            while (isdigit(line[idx])) temp[j++] = line[idx++];
        }
        else {
            temp[j++] = line[idx++];
        }
        temp[j] = '\0';
        int w;
        TTF_SizeText(font, temp, &w, NULL);
        total_width += w;
    }

    if (total_width == 0) total_width = 1;

    SDL_Surface *final_surface = SDL_CreateRGBSurface(
        0, total_width, height, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
    );
    if (!final_surface) return;

    SDL_FillRect(final_surface, NULL,
        SDL_MapRGBA(final_surface->format, 0, 0, 0, 0));

    int offset_x = 0;
    idx = 0;

    while (line[idx]) {
        char temp[128];
        int j = 0;
        SDL_Color color = {255, 255, 255, 255};

        if (line[idx] == '/' && line[idx+1] == '/') {
            strcpy(temp, &line[idx]);
            color = (SDL_Color){100, 200, 100, 255};
            idx += strlen(&line[idx]);
        }
        else if (isalpha(line[idx])) {
            while (isalnum(line[idx])) temp[j++] = line[idx++];
            temp[j] = '\0';
            if (is_keyword(temp))
                color = (SDL_Color){80, 160, 255, 255};
        }
        else if (isdigit(line[idx])) {
            while (isdigit(line[idx])) temp[j++] = line[idx++];
            temp[j] = '\0';
            color = (SDL_Color){255, 200, 100, 255};
        }
        else {
            temp[j++] = line[idx++];
            temp[j] = '\0';
        }

        SDL_Surface *s = TTF_RenderText_Blended(font, temp, color);
        if (!s) continue;

        SDL_Rect dst = {offset_x, 0, s->w, s->h};
        SDL_BlitSurface(s, NULL, final_surface, &dst);
        offset_x += s->w;
        SDL_FreeSurface(s);
    }

    buf->line_textures[i] = SDL_CreateTextureFromSurface(renderer, final_surface);
    SDL_FreeSurface(final_surface);
}

void open_file_in_tab(const char *filename) {
	if (tab_count >= MAX_TABS) return;

	EditorBuffer *buf = &tabs[tab_count];
	memset(buf, 0, sizeof(EditorBuffer));

	strcpy(buf->filename, filename);

	FILE *fp = fopen(filename, "r");
	if (!fp) {
		// new empty buffer
		strcpy(buf->lines[0], "");
		buf->line_count = 1;
		buf->dirty_flags[0] = 1;
		buf->line_textures[0] = NULL;
	} else {
		buf->line_count = 0;
		char buffer[MAX_COLS];
		while (fgets(buffer, MAX_COLS, fp) && buf->line_count < MAX_LINES) {
			buffer[strcspn(buffer, "\n")] = '\0';
			strcpy(buf->lines[buf->line_count], buffer);
			buf->dirty_flags[buf->line_count] = 1;
			buf->line_textures[buf->line_count] = NULL;
			buf->line_count++;
		}
		fclose(fp);
		if (buf->line_count == 0) {
			strcpy(buf->lines[0], "");
			buf->line_count = 1;
		}
	}

	buf->cursor_row = 0;
	buf->cursor_col = 0;
	buf->scroll_offset = 0;
	buf->undo_top = -1;
	buf->redo_top = -1;
	buf->selection.active = 0;

	active_tab = tab_count;
	tab_count++;
}

void save_file(EditorBuffer *buf)
{
	const char *filename = buf->filename;
	if(strlen(filename)==0) filename="output.txt";
	FILE *fp=fopen(filename,"w");
	if(!fp){
		printf("Could not save file\n");
		return;
	}
	for(int i=0;i<buf->line_count;i++){
		fprintf(fp,"%s\n",buf->lines[i]);
	}
	fclose(fp);
	printf("Saved: %s\n",filename);
}

void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y) {
    if (!font || !text) return;
    SDL_Color color = {255, 255, 255, 255};
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        printf("Texture Error: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

void find_next(EditorBuffer *buf){
	for(int i=buf->cursor_row;i<buf->line_count;i++){
		char *pos=strstr(buf->lines[i],search_query);
		if(pos){
			match_row=i;
			match_col=pos-buf->lines[i];

			buf->cursor_row=i;
			buf->cursor_col=match_col;
			return;
		}
	}
}

// ---- MAIN ------
int main(int argc,char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    load_directory(".");

    SDL_Window *window = SDL_CreateWindow(
        "Ekagra Editor",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font *font = TTF_OpenFont("/Users/ekagraagrawal/Library/Fonts/FiraCode-Regular.ttf", 24);
    if(!font){
        printf("Font Error: %s\n",TTF_GetError());
        return 1;
    }

    if(argc>1){
        open_file_in_tab(argv[1]);
    } else {
        // open a blank buffer
        open_file_in_tab("untitled");
    }

    SDL_StartTextInput();

    int running = 1;
    SDL_Event event;

    // ---- MAIN LOOP----
    while (running) {

        EditorBuffer *buf = &tabs[active_tab];

        while (SDL_PollEvent(&event)) {

            if (event.type == SDL_QUIT) running = 0;

            // ---------- TEXT INPUT ----------
            if (event.type == SDL_TEXTINPUT) {
                if(buf->selection.active){
                    delete_selection(buf);
                }
		if(search_mode){
			if(search_len<sizeof(search_query)-1){
				strcat(search_query,event.text.text);
				search_len++;
			}
			break;//change
		}
                save_state(buf);
                buf->dirty_flags[buf->cursor_row]=1;
                char *line = buf->lines[buf->cursor_row];
                int len = strlen(line);
                if (len < MAX_COLS - 1) {
                    memmove(&line[buf->cursor_col + 1],
                            &line[buf->cursor_col],
                            len - buf->cursor_col + 1);
                    line[buf->cursor_col] = event.text.text[0];
                    buf->cursor_col++;
                }
            }

            // ----MOUSE EVENT-----
            if(event.type == SDL_MOUSEWHEEL){
                if(event.wheel.y>0 && buf->scroll_offset>0){
                    buf->scroll_offset--;
                }
                if(event.wheel.y<0 && buf->scroll_offset<buf->line_count-1) buf->scroll_offset++;
            }

            // ---- KEY EVENTS -----
            if (event.type == SDL_KEYDOWN) {
                int shift = event.key.keysym.mod&KMOD_SHIFT;
		if(search_mode && event.key.keysym.sym==SDLK_ESCAPE){search_mode=0;
		}
		if(search_mode && event.key.keysym.sym==SDLK_RETURN){
			EditorBuffer *buf=&tabs[active_tab];
			find_next(buf);
		}
		//CTRL + F - search
		if((event.key.keysym.mod & KMOD_CTRL)&&event.key.keysym.sym==SDLK_f){
		search_mode=1;
		search_len=0;
		search_query[0]='\0';	
			
		}
                // CTRL + TAB — switch tabs
                if((event.key.keysym.mod & KMOD_CTRL) && event.key.keysym.sym == SDLK_TAB){
                    if(shift){
                        active_tab = (active_tab - 1 + tab_count) % tab_count;
                    } else {
                        active_tab = (active_tab + 1) % tab_count;
                    }
                    buf = &tabs[active_tab];
                    continue;
                }

                // CTRL + S
                if((event.key.keysym.sym==SDLK_s)&&(event.key.keysym.mod & KMOD_CTRL)){
                    save_file(buf);
                }

                // BACKSPACE
                if (event.key.keysym.sym == SDLK_BACKSPACE) {
                    if(buf->selection.active){
                        delete_selection(buf);
                        break;
                    }
		    if(search_mode){
		    	if(search_len>0){
				search_query[--search_len]='\0';
				break; //change
			}
		    }
                    save_state(buf);
                    buf->dirty_flags[buf->cursor_row]=1;
                    if (buf->cursor_col > 0) {
                        char *line = buf->lines[buf->cursor_row];
                        memmove(&line[buf->cursor_col - 1],
                                &line[buf->cursor_col],
                                strlen(line) - buf->cursor_col + 1);
                        buf->cursor_col--;
                    } else if (buf->cursor_row > 0) {
                        int prev_len = strlen(buf->lines[buf->cursor_row - 1]);
                        buf->dirty_flags[buf->cursor_row-1]=1;
                        strcat(buf->lines[buf->cursor_row - 1], buf->lines[buf->cursor_row]);
                        for (int i = buf->cursor_row; i < buf->line_count - 1; i++) {
                            buf->line_textures[i]=buf->line_textures[i+1];
                            buf->dirty_flags[i]=1;
                            strcpy(buf->lines[i], buf->lines[i + 1]);
                        }
                        buf->line_textures[buf->line_count-1]=NULL;
                        buf->line_count--;
                        buf->cursor_row--;
                        buf->cursor_col = prev_len;
                    }
                }

                // CTRL + Z (UNDO)
                if((event.key.keysym.mod&KMOD_CTRL)&& event.key.keysym.sym==SDLK_z){
                    if(buf->undo_top>=0){
                        if(buf->redo_top<MAX_HISTORY-1){
                            buf->redo_top++;
                            EditorState *r= &buf->redo_stack[buf->redo_top];
                            for(int i=0;i<buf->line_count;i++){
                                strcpy(r->lines[i],buf->lines[i]);
                            }
                            r->line_count=buf->line_count;
                            r->cursor_row=buf->cursor_row;
                            r->cursor_col=buf->cursor_col;
                        }
                        restore_state(buf, &buf->undo_stack[buf->undo_top]);
                        buf->undo_top=buf->undo_top-1;
                    }
                }

                // CTRL + Y (REDO)
                if((event.key.keysym.mod&KMOD_CTRL)&&event.key.keysym.sym==SDLK_y){
                    if(buf->redo_top>=0){
                        if(buf->undo_top<MAX_HISTORY-1){
                            buf->undo_top++;
                            EditorState *u = &buf->undo_stack[buf->undo_top];
                            for(int i=0;i<buf->line_count;i++){
                                strcpy(u->lines[i],buf->lines[i]);
                            }
                            u->line_count=buf->line_count;
                            u->cursor_row=buf->cursor_row;
                            u->cursor_col=buf->cursor_col;
                        }
                        restore_state(buf, &buf->redo_stack[buf->redo_top]);
                        buf->redo_top--;
                    }
                }

                // ENTER
                if (event.key.keysym.sym == SDLK_RETURN) {
                    if(buf->selection.active){
                        delete_selection(buf);
                    }
                    save_state(buf);
                    buf->dirty_flags[buf->cursor_row]=1;
                    buf->dirty_flags[buf->cursor_row+1]=1;
                    char *line = buf->lines[buf->cursor_row];
                    for (int i = buf->line_count; i > buf->cursor_row + 1; i--) {
                        buf->line_textures[i]=buf->line_textures[i-1];
                        strcpy(buf->lines[i], buf->lines[i - 1]);
                    }
                    buf->line_textures[buf->cursor_row+1]=NULL;
                    strcpy(buf->lines[buf->cursor_row + 1], &line[buf->cursor_col]);
                    line[buf->cursor_col] = '\0';
                    buf->line_count++;
                    buf->cursor_row++;
                    buf->cursor_col = 0;
                }

                // ARROWS
                if (event.key.keysym.sym == SDLK_LEFT) {
                    if(!shift) buf->selection.active=0;
                    if (buf->cursor_col > 0) buf->cursor_col--;
                    if (shift) {
                        if (!buf->selection.active) {
                            buf->selection.start_row = buf->cursor_row;
                            buf->selection.start_col = buf->cursor_col + 1;
                            buf->selection.active = 1;
                        }
                        buf->selection.end_row = buf->cursor_row;
                        buf->selection.end_col = buf->cursor_col;
                    }
                }

                if (event.key.keysym.sym == SDLK_RIGHT) {
                    if(!shift) buf->selection.active=0;
                    if (buf->cursor_col < (int)strlen(buf->lines[buf->cursor_row])) buf->cursor_col++;
                    if(shift){
                        if(!buf->selection.active){
                            buf->selection.start_row=buf->cursor_row;
                            buf->selection.start_col=buf->cursor_col-1;
                            buf->selection.active=1;
                        }
                        buf->selection.end_row=buf->cursor_row;
                        buf->selection.end_col=buf->cursor_col;
                    }
                }

                if (event.key.keysym.sym == SDLK_UP) {
                    if (!shift) buf->selection.active = 0;
                    if (buf->cursor_row > 0) {
                        buf->cursor_row--;
                        buf->cursor_col = SDL_min(buf->cursor_col, (int)strlen(buf->lines[buf->cursor_row]));
                    }
                    if (shift) {
                        if (!buf->selection.active) {
                            buf->selection.start_row = buf->cursor_row+1;
                            buf->selection.start_col = buf->cursor_col;
                            buf->selection.active = 1;
                        }
                        buf->selection.end_row = buf->cursor_row;
                        buf->selection.end_col = buf->cursor_col;
                    }
                }

                // CTRL + C (COPY)
                if ((event.key.keysym.mod & KMOD_CTRL) && event.key.keysym.sym == SDLK_c) {
                    Selection s = buf->selection;
                    normalize_selection(&s);
                    char clipboard[4096] = "";
                    for (int i = s.start_row; i <= s.end_row; i++) {
                        int start = (i == s.start_row) ? s.start_col : 0;
                        int end   = (i == s.end_row)   ? s.end_col   : strlen(buf->lines[i]);
                        strncat(clipboard, &buf->lines[i][start], end - start);
                        if (i != s.end_row)
                            strcat(clipboard, "\n");
                    }
                    SDL_SetClipboardText(clipboard);
                }

                // CTRL + V (PASTE)
                if ((event.key.keysym.mod & KMOD_CTRL) && event.key.keysym.sym == SDLK_v) {
                    save_state(buf);
                    char *clip = SDL_GetClipboardText();
                    if (!clip) continue;
                    if(buf->selection.active){
                        delete_selection(buf);
                    }
                    char *line = buf->lines[buf->cursor_row];
                    char *newline= strchr(clip,'\n');
                    if(!newline){
                        int len = strlen(line);
                        if (len + strlen(clip) < MAX_COLS) {
                            memmove(&line[buf->cursor_col + strlen(clip)],
                                    &line[buf->cursor_col],
                                    len - buf->cursor_col + 1);
                            memcpy(&line[buf->cursor_col], clip, strlen(clip));
                            buf->cursor_col += strlen(clip);
                            buf->dirty_flags[buf->cursor_row] = 1;
                        }
                        SDL_free(clip);
                    }
                    else{
                        char first[MAX_COLS], last[MAX_COLS];
                        strncpy(first,clip,newline-clip);
                        first[newline-clip]='\0';
                        strcpy(last,&newline[1]);
                        char tail[MAX_COLS];
                        strcpy(tail,&line[buf->cursor_col]);
                        line[buf->cursor_col]='\0';
                        strcat(line,first);
                        buf->dirty_flags[buf->cursor_row]=1;
                        for(int i=buf->line_count;i>buf->cursor_row+1;i--){
                            strcpy(buf->lines[i],buf->lines[i-1]);
                        }
                        strcpy(buf->lines[buf->cursor_row+1],last);
                        strcat(buf->lines[buf->cursor_row+1],tail);
                        buf->line_count++;
                        buf->cursor_row++;
                        buf->cursor_col=strlen(last);
                        buf->dirty_flags[buf->cursor_row]=1;
                        SDL_free(clip);
                    }
                }

                // SIDEBAR OPERATION (ALT keys)
                if(event.key.keysym.mod&KMOD_ALT){
                    if(event.key.keysym.sym==SDLK_RETURN){
                        open_file_in_tab(files[selected_file]);
                        buf = &tabs[active_tab];
                    }
                    if(event.key.keysym.sym==SDLK_UP&&selected_file>0){
                        selected_file--;
                    }
                    if(event.key.keysym.sym==SDLK_DOWN&&selected_file<file_count-1){
                        selected_file++;
                    }
                }

                if (event.key.keysym.sym == SDLK_DOWN) {
                    if (!shift) buf->selection.active = 0;
                    if (buf->cursor_row < buf->line_count - 1) {
                        buf->cursor_row++;
                        buf->cursor_col = SDL_min(buf->cursor_col, (int)strlen(buf->lines[buf->cursor_row]));
                    }
                    if (shift) {
                        if (!buf->selection.active) {
                            buf->selection.start_row = buf->cursor_row-1;
                            buf->selection.start_col = buf->cursor_col;
                            buf->selection.active = 1;
                        }
                        buf->selection.end_row = buf->cursor_row;
                        buf->selection.end_col = buf->cursor_col;
                    }
                }
            }
        }

        // re-fetch buf after event processing (tab may have switched)
        buf = &tabs[active_tab];

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        int line_height = TTF_FontHeight(font);

        int window_height, window_width;
        SDL_GetWindowSize(window, &window_width, &window_height);

        int visible_lines = (window_height - 50 - editor_y_offset) / line_height;

        // scroll logic
        if (buf->cursor_row < buf->scroll_offset) {
            buf->scroll_offset = buf->cursor_row;
        }
        if (buf->cursor_row >= buf->scroll_offset + visible_lines) {
            buf->scroll_offset = buf->cursor_row - visible_lines + 1;
        }

        render_sidebar(renderer, font);
        render_tabs(renderer, font);
	// highlight matches
	int scroll_offset= buf->scroll_offset;  
	if (search_mode && strlen(search_query) > 0) {

	    for (int i = scroll_offset; i < scroll_offset + visible_lines; i++) {

		if (i >= buf->line_count) break;

		char *pos = strstr(buf->lines[i], search_query);

		if (pos) {
		    int col = pos - buf->lines[i];

		    char temp[MAX_COLS];
		    strncpy(temp, buf->lines[i], col);
		    temp[col] = '\0';

		    int offset;
		    TTF_SizeText(font, temp, &offset, NULL);

		    int match_w;
		    TTF_SizeText(font, search_query, &match_w, NULL);

		    int x = editor_x + offset;
		    int y = editor_y_offset + (i - scroll_offset) * line_height;

		    SDL_Rect rect = {x, y, match_w, line_height+7};

		    SDL_SetRenderDrawColor(renderer, 120, 120, 40, 255);
		    SDL_RenderFillRect(renderer, &rect);
		}
	    }
	}

	if (search_mode) {

	    SDL_Rect bar = {260, 0, 400, 30};
	    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
	    SDL_RenderFillRect(renderer, &bar);

	    char display[160];
	    sprintf(display, "Search: %s", search_query);

	    SDL_Color color = {255, 255, 255, 255};

	    SDL_Surface *s = TTF_RenderText_Blended(font, display, color);
	    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);

	    SDL_Rect dst = {270, 5, s->w, s->h};
	    SDL_RenderCopy(renderer, t, NULL, &dst);

	    SDL_FreeSurface(s);
	    SDL_DestroyTexture(t);
	}

	for (int i = 0; i < visible_lines; i++) {
            int line_index = buf->scroll_offset + i;
            if (line_index >= buf->line_count) break;

            int y = editor_y_offset + 10 + i * line_height;

            if (buf->dirty_flags[line_index]) {
                update_line_texture(renderer, font, buf, line_index);
                buf->dirty_flags[line_index] = 0;
            }

            // SELECTION RENDERING
            if (buf->selection.active) {
                Selection s = buf->selection;
                normalize_selection(&s);

                if (line_index >= s.start_row && line_index <= s.end_row) {
                    int start_x = 260;
                    int end_x = 260;
                    char temp[MAX_COLS];

                    if (line_index == s.start_row) {
                        strncpy(temp, buf->lines[line_index], s.start_col);
                        temp[s.start_col] = '\0';
                        int w;
                        TTF_SizeText(font, temp, &w, NULL);
                        start_x += w;
                    }

                    if (line_index == s.end_row) {
                        strncpy(temp, buf->lines[line_index], s.end_col);
                        temp[s.end_col] = '\0';
                        int w;
                        TTF_SizeText(font, temp, &w, NULL);
                        end_x += w;
                    } else {
                        int w;
                        TTF_SizeText(font, buf->lines[line_index], &w, NULL);
                        end_x += w;
                    }

                    SDL_Rect rect = {
                        start_x,
                        y,
                        end_x - start_x,
                        line_height
                    };

                    SDL_SetRenderDrawColor(renderer, 60, 60, 120, 255);
                    SDL_RenderFillRect(renderer, &rect);
                }
            }

            // TEXTURE RENDERING
            if (buf->line_textures[line_index]) {
                int w, h;
                SDL_QueryTexture(buf->line_textures[line_index], NULL, NULL, &w, &h);
                SDL_Rect dst = {editor_x, y, w, h};
                SDL_RenderCopy(renderer, buf->line_textures[line_index], NULL, &dst);
            }
        }

        // ---- CURSOR ------
        char temp[MAX_COLS];
        strncpy(temp,buf->lines[buf->cursor_row],buf->cursor_col);
        temp[buf->cursor_col]='\0';
        int cursor_offset=0;
        TTF_SizeText(font,temp,&cursor_offset,NULL);

        int cursor_x = 260 + cursor_offset;
        int cursor_y = editor_y_offset + 10 + (buf->cursor_row - buf->scroll_offset) * line_height;
        Uint32 current_time = SDL_GetTicks();
        int show_cursor = (current_time/CURSOR_BLINK_INTERVAL)%2==0;
        if(show_cursor){
            SDL_Rect cursor_rect = {cursor_x, cursor_y, 2, 24};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &cursor_rect);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // ------ CLEANUP -----
    for(int t=0;t<tab_count;t++){
        for(int i=0;i<MAX_LINES;i++){
            if(tabs[t].line_textures[i]){
                SDL_DestroyTexture(tabs[t].line_textures[i]);
            }
        }
    }
    SDL_StopTextInput();
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
