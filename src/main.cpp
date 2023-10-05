#include <iostream>
#include <span>
#include "logger.hpp"
#include "renderLib.hpp"
#include "engineInternals.hpp"

bool InitializeDependencies();

int main(int argc, char* args[]){
    if(argc > 2 || !InitializeDependencies()) return -1;

	{
		AppManager appWindow = AppManager(1000, 500, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED, "Tools");

		MainLoop(appWindow, std::span{args, (size_t)argc});
	}

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}

bool InitializeDependencies(){
	if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) < 0 )
	{
        printf("Unable to initialize SDL: %s\n", SDL_GetError());
		return false;
    }       

	int imgFlags = IMG_INIT_PNG;
	if( !( IMG_Init( imgFlags ) & imgFlags ) )
	{
		printf( "SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError() );
		return false;
	}
	
	if( TTF_Init() == -1 )
	{
		printf( "SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError() );
		return false;
	}

	return true;
}