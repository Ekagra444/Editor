#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINES 100
#define MAX_COLS  256
#define CURSOR_BLINK_INTERVAL 700
SDL_Texture* line_textures[MAX_LINES];
int dirty[MAX_LINES];

char lines[MAX_LINES][MAX_COLS];
int line_count = 1;
//current file name
char current_file[256]="";
int cursor_row = 0;
int cursor_col = 0;

// -----Texture creation per line------
void update_line_texture(SDL_Renderer *renderer, TTF_Font *font, int i)
{
	if(line_textures[i]){
		SDL_DestroyTexture(line_textures[i]);
		line_textures[i]=NULL;
	}
	SDL_Color color= {255,255,255,255};
	SDL_Surface *surface = TTF_RenderText_Blended(font,lines[i],color);
	if(!surface){return;}
	line_textures[i]=SDL_CreateTextureFromSurface(renderer,surface);
	SDL_FreeSurface(surface);
}

////------FILE LOADING AND SAVING--------
void load_file(const char* filename)
{
	FILE *fp=fopen(filename,"r");
	if(!fp)
	{
		printf("Could not open file \n");
		return;
	}
	strcpy(current_file,filename);
	line_count=0;
	char buffer[MAX_COLS];
	while(fgets(buffer,MAX_COLS,fp) && line_count<MAX_LINES)
	{
		buffer[strcspn(buffer,"\n")]='\0';
		strcpy(lines[line_count],buffer);
		line_count++;
	}
	fclose(fp);
	if(line_count==0)
	{
		strcpy(lines[0],"");
		line_count=1;
	}
	for(int i=0;i<line_count;i++){
		dirty[i]=1;
	}
	cursor_row=0;
	cursor_col=0;
}

void save_file(const char *filename)
{
	FILE *fp=fopen(filename,"w");
	if(!fp)
	{
		printf("Could not save file\n");
		return;
	}
	for(int i=0;i<line_count;i++){
		fprintf(fp,"%s\n",lines[i]);
	}
	fclose(fp);
	printf("Saved: %s\n",filename);
}
//
// ---------- TEXT RENDER ----------
void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y) {

    if (!font || !text) return;  // guard

    SDL_Color color = {255, 255, 255, 255};

    SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);

    if (!surface) {
//        printf("TTF Render Error: %s\n", TTF_GetError());
        return;
    }

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
// ---- MAIN ------
int main(int argc,char *argv[]) {
    strcpy(lines[0], "");
    if(argc>1){
    	load_file(argv[1]);
    }	
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

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

    SDL_StartTextInput();

    int running = 1;
    SDL_Event event;

    //state initialization
    for(int i=0;i<MAX_LINES;i++){
    	line_textures[i]=NULL;
	dirty[i]=1;
    }
    // ---- MAIN LOOP----
    while (running) {

        while (SDL_PollEvent(&event)) {

            if (event.type == SDL_QUIT) running = 0;

            // ---------- TEXT INPUT ----------
            if (event.type == SDL_TEXTINPUT) {
		dirty[cursor_row]=1; // create texture again
                char *line = lines[cursor_row];
                int len = strlen(line);

                if (len < MAX_COLS - 1) {
                    memmove(&line[cursor_col + 1],
                            &line[cursor_col],
                            len - cursor_col + 1);

                    line[cursor_col] = event.text.text[0];
                    cursor_col++;
                }
            }

            // ---- KEY EVENTS -----
            if (event.type == SDL_KEYDOWN) {
		//CTRL + S 
		if((event.key.keysym.sym==SDLK_s)&&(event.key.keysym.mod & KMOD_CTRL)){
			if(strlen(current_file)>0){
				save_file(current_file);
			}
			else{
				save_file("output.txt");
			}
		
		}
                // BACKSPACE
                if (event.key.keysym.sym == SDLK_BACKSPACE) {
	   	    dirty[cursor_row]=1;
                    if (cursor_col > 0) {
                        char *line = lines[cursor_row];

                        memmove(&line[cursor_col - 1],
                                &line[cursor_col],
                                strlen(line) - cursor_col + 1);

                        cursor_col--;

                    } else if (cursor_row > 0) {

                        int prev_len = strlen(lines[cursor_row - 1]);
			dirty[cursor_row-1]=1;
                        strcat(lines[cursor_row - 1], lines[cursor_row]);

                        for (int i = cursor_row; i < line_count - 1; i++) {
			    line_textures[i]=line_textures[i+1];
			    dirty[i]=1;
                            strcpy(lines[i], lines[i + 1]);
                        }
			line_textures[line_count-1]=NULL;

                        line_count--;
                        cursor_row--;
                        cursor_col = prev_len;
                    }
                }

                // ENTER
                if (event.key.keysym.sym == SDLK_RETURN) {
	   	    dirty[cursor_row]=1;
	   	    dirty[cursor_row+1]=1;
                    char *line = lines[cursor_row];
                    for (int i = line_count; i > cursor_row + 1; i--) {
                       	 line_textures[i]=line_textures[i-1];
		 	 strcpy(lines[i], lines[i - 1]);
                    }
		    line_textures[cursor_row+1]=NULL;
                    strcpy(lines[cursor_row + 1], &line[cursor_col]);
                    line[cursor_col] = '\0';

                    line_count++;
                    cursor_row++;
                    cursor_col = 0;
                }

                // ARROWS
                if (event.key.keysym.sym == SDLK_LEFT) {
                    if (cursor_col > 0) cursor_col--;
                }

                if (event.key.keysym.sym == SDLK_RIGHT) {
                    if (cursor_col < strlen(lines[cursor_row])) cursor_col++;
                }

                if (event.key.keysym.sym == SDLK_UP) {
                    if (cursor_row > 0) {
                        cursor_row--;
                        cursor_col = SDL_min(cursor_col, strlen(lines[cursor_row]));
                    }
                }

                if (event.key.keysym.sym == SDLK_DOWN) {
                    if (cursor_row < line_count - 1) {
                        cursor_row++;
                        cursor_col = SDL_min(cursor_col, strlen(lines[cursor_row]));
                    }
                }
            }
        }

        //------ RENDER------
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
	
	int line_height = TTF_FontHeight(font);
         
	for (int i = 0; i < line_count; i++) {
        	if(dirty[i]){
			update_line_texture(renderer,font,i);
			dirty[i]=0;
		}
		if(line_textures[i]){
			int w,h;
			SDL_QueryTexture(line_textures[i],NULL,NULL,&w,&h);
			SDL_Rect dst={50,50+i*line_height,w,h};
			SDL_RenderCopy(renderer,line_textures[i],NULL,&dst);

		}
	}

        // ---- CURSOR ------
	
	char temp[MAX_COLS];
	strncpy(temp,lines[cursor_row],cursor_col);
	temp[cursor_col]='\0';
	int cursor_offset=0;
	TTF_SizeText(font,temp,&cursor_offset,NULL);

        int cursor_x = 50 + cursor_offset;
        int cursor_y = 50 + cursor_row * line_height;
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
    for(int i=0;i<MAX_LINES;i++){
    	if(line_textures[i]){
		SDL_DestroyTexture(line_textures[i]);
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
