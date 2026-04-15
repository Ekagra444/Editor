#include <SDL.h>
#include<SDL_ttf.h>
#include <stdio.h>
void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y){
	SDL_Color color= {255,255,255,255};
	if(!(*text))return;
	SDL_Surface *surface= TTF_RenderText_Blended(font,text,color);
	if (!surface) {
	    printf("Surface Error: %s\n", TTF_GetError());
	    return;
	}
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer,surface);
	if (surface && !texture) {
	    SDL_FreeSurface(surface);
	    printf("Texture Error: %s\n", SDL_GetError());
	    return;
	}
	SDL_Rect dst = {x,y,surface->w,surface->h};
	SDL_FreeSurface(surface);//surface no longer needed
	SDL_RenderCopy(renderer,texture,NULL,&dst);
	SDL_DestroyTexture(texture);
}
int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL Init Error: %s\n", SDL_GetError());
        return 1;
    }
    if(TTF_Init()==-1){
	    printf("TTF Init Error: %s\n",TTF_GetError());
	    return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Ekagra Editor",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );
    TTF_Font *font = TTF_OpenFont("/Users/ekagraagrawal/Library/Fonts/FiraCode-Regular.ttf", 24);

    if (!font) {
	printf("Font Error: %s\n", TTF_GetError());
	return 1;
    }

    if (!window) {
        printf("Window Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(
	    window,
	    -1,
	    SDL_RENDERER_ACCELERATED
     );

    int running =1;
    SDL_Event event;
    SDL_StartTextInput();
    char buffer[1024] = ""; // editor state 
    while(running){
    	while(SDL_PollEvent(&event)){
		if(event.type==SDL_QUIT){running =0;}
		if(event.type==SDL_TEXTINPUT){
			strcat(buffer,event.text.text);
		}
		if(event.type==SDL_KEYDOWN){
			if(event.key.keysym.sym==SDLK_BACKSPACE && strlen(buffer)>0){
			buffer[strlen(buffer)-1]='\0';
			}
		}
		
	}
    	SDL_SetRenderDrawColor(renderer,30,30,30,255);
	SDL_RenderClear(renderer);
//rendering text	
	render_text(renderer,font,buffer,50,50);
    	
	SDL_RenderPresent(renderer);
    }
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;

}




