#include "renderLib.hpp"
#include "logger.hpp"
#include <ctime>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <algorithm>

SDL_Point GetSizeOfPNG(const char *path){
	SDL_Point result = {0, 0};
	std::ifstream png(path, std::ios::binary);

	if(!png.is_open()){
		std::cout << "\nCannot open png " << path << '\n';
		return result;
	}

	//We check the first 8 bytes to make sure it is a png
	{
		unsigned char magicNumber[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
		unsigned char pngHeader[8] = {0, 0, 0, 0, 0, 0, 0, 0};

		png.read((char*)pngHeader, 8);
	
		for(int i = 0; i < 8; ++i){
			if(pngHeader[i] != magicNumber[i]){
				std::cout << '\n' << path << " is not a png\n";
				return result;
			}
		}
	}

	//We check that the first chunk is the IHDR chunk and then read the width and height
	{
		unsigned char chunkLength[4] = {0, 0, 0, 0};
		unsigned char chunkType[4] = {0, 0, 0, 0};

		unsigned char IHDRLength[4] = {0x00, 0x00, 0x00, 0x0D};
		unsigned char IHDRType[4] = {0x49, 0x48, 0x44, 0x52};

		unsigned char readWidth[4] = {0, 0, 0, 0};
		unsigned char readHeight[4] = {0, 0, 0, 0};

		png.read((char*)chunkLength, 4);
		png.read((char*)chunkType, 4);
		png.read((char*)readWidth, 4);
		png.read((char*)readHeight, 4);

		//Check that the first chunk is the IHDR
		for(int i = 0; i < 4; ++i){
			if(chunkLength[i] != IHDRLength[i]){
				std::cout << '\n' << path << " chunk length does not match\n";
				return result;
			}
		}
		for(int i = 0; i < 4; ++i){
			if(chunkType[i] != IHDRType[i]){
				std::cout << '\n' << path << " chunk type does not match\n";
				return result;
			}
		}

		//We set the result based on the read values, we have to do it this way as it's in big endian
		for(int i = 0; i < 4; ++i){
			result.x |= (int)readWidth[i] << (8*(3-i));
			result.y |= (int)readHeight[i] << (8*(3-i));
		}
	}

	png.close();

	return result;
}
SDL_Point GetSizeOfBMP(const char *path){
	SDL_Point result = {0, 0};
	std::ifstream bmp(path, std::ios::binary);

	if(!bmp.is_open()){
		std::cout << "\nCannot open bmp" << path << '\n';
		return result;
	}

	//TODO: check that it is indeed a bmp, we currently don't as I find there to be a lot of bmp formats. Currently using "Windows BITMAPINFOHEADER" and jumping straight into the size data
	bmp.ignore(0x12);

	{
		char readWidth[4] = {0, 0, 0, 0};
		char readHeight[4] = {0, 0, 0, 0};

		bmp.read(readWidth, 4);
		bmp.read(readHeight, 4);
		
		result.x = *(int*)readWidth;
		result.y = *(int*)readHeight;
	}

	bmp.close();

	return result;
}

void FreeSurface(SDL_Surface *surface){
	SDL_FreeSurface(surface);
	surface = nullptr;
}

void DestroyTexture(SDL_Texture *texture){
	SDL_DestroyTexture(texture);
	texture = nullptr;
}

void CloseFont(TTF_Font *font){
    TTF_CloseFont( font );
    font = nullptr;
}

void Release(SDL_Window *&window, SDL_Renderer *&renderer){
	SDL_DestroyRenderer( renderer );
	SDL_DestroyWindow( window );
	renderer = nullptr;
	window = nullptr;
    TTF_Quit();
	IMG_Quit();
    SDL_Quit();
}

SDL_Surface *LoadSurface(const char *path, SDL_Surface *windowSurface, Format format){
	SDL_Surface *result = nullptr, *loaded;
	
	switch(format){
		case Format::BMP: loaded = SDL_LoadBMP(path); break;
		case Format::PNG: loaded = IMG_Load(path); break;
		default: printf("Invalid type of format %i\n", format); break;
	}

    result = SDL_ConvertSurface(loaded, windowSurface->format, 0);
	
    SDL_FreeSurface(loaded);

	return result;
}

SDL_Texture* LoadTexture(const char *path, SDL_Renderer *renderer, Format format){
	SDL_Texture* texture = nullptr;
	SDL_Surface* image = nullptr;
	switch(format){
		case Format::BMP: image = SDL_LoadBMP(path); break;
		case Format::PNG: image = IMG_Load(path); break;
		default: printf("Invalid type of format %i\n", format); break;
	}
	
    if( image == nullptr )
    {
        std::cout<<"Failed to load image at "<<path<<": "<<SDL_GetError()<<'\n';
    }
	else
	{
        texture = SDL_CreateTextureFromSurface( renderer, image );
        if( texture == NULL )
        {
            std::cout<<"Unable to create texture from "<<path<<"! SDL Error: "<<SDL_GetError()<<'\n';
        }
		
		SDL_FreeSurface( image );
	}

	return texture;
}

SDL_Texture* LoadTexture(const char *path, SDL_Renderer *renderer, SDL_Color colorKey, Format format){
	SDL_Texture* texture = nullptr;
	SDL_Surface* image = nullptr;
	switch(format){
		case Format::BMP: image = SDL_LoadBMP(path); break;
		case Format::PNG: image = IMG_Load(path); break;
		default: printf("Invalid type of format %i\n", format); break;
	}
	
    if( image == nullptr )
    {
        std::cout<<"Failed to load image at "<<path<<": "<<SDL_GetError()<<'\n';
    }
	else
	{
        SDL_SetColorKey( image, SDL_TRUE, SDL_MapRGB( image->format, colorKey.r, colorKey.g, colorKey.b ) );
		
        texture = SDL_CreateTextureFromSurface( renderer, image );
        if( texture == NULL )
        {
            std::cout<<"Unable to create texture from "<<path<<"! SDL Error: "<<SDL_GetError()<<'\n';
        }

		SDL_FreeSurface( image );
	}

	return texture;
}

SDL_Texture* NFLoadTexture(std::string path, SDL_Renderer *renderer){
	int length = path.size();
	Format format = Format::PNG;
	std::string stringFormat(path[length-4], path[length-1]);
	if(stringFormat == "bmp"){
		format = Format::BMP;
	}
	return LoadTexture(path.c_str(), renderer, format);
}

SDL_Texture* NFLoadTexture(std::string path, SDL_Renderer *renderer, SDL_Color keyColor){
	int length = path.size();
	Format format = Format::PNG;
	std::string stringFormat(path[length-4], path[length-1]);
	if(stringFormat == "bmp"){
		format = Format::BMP;
	}
	return LoadTexture(path.c_str(), renderer, keyColor, format);
}

SDL_Texture* LoadTextureFromText(const char *text, SDL_Renderer *renderer, TTF_Font *font, SDL_Color colorText){
	SDL_Texture *resultingTexture = nullptr;
	SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, colorText);
	if(textSurface == nullptr){
		printf( "Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError() );
	}
	else{
		resultingTexture = SDL_CreateTextureFromSurface( renderer, textSurface );
		if(resultingTexture == nullptr){
			printf( "Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError() );
		}
        SDL_FreeSurface( textSurface );
	}
	return resultingTexture;
}

TTF_Font* LoadFont(std::string path, int size){
	TTF_Font *createdFont = TTF_OpenFont( path.c_str(), size );
	if( createdFont == nullptr )
    {
        printf( "Failed to load font! SDL_ttf Error: %s\n", TTF_GetError() );
    }
	return createdFont;
}

void DisplayTexture(SDL_Renderer *renderer, SDL_Texture *texture){
	SDL_RenderCopy(renderer, texture, nullptr, nullptr);
}

void DisplayTexture(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *renderingPart){
	SDL_RenderCopy(renderer, texture, nullptr, renderingPart);
}

void GetTextureSize(SDL_Texture *texture, int *width, int *height, float scale){
	SDL_QueryTexture(texture, nullptr, nullptr, width, height);
	*width *= scale; *height *= scale;
}

void GetTextureSize(SDL_Texture *texture, unsigned int *width, unsigned int *height, float scale){
	int w, h;
	SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
	*width = w*scale; *height = h*scale;
}

void GetTextureSize(SDL_Texture *texture, float *width, float *height, float scale){
	int w, h;
	SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
	*width = w*scale; *height = h*scale;
}

void ClearRender(SDL_Renderer* renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	SDL_SetRenderDrawColor( renderer, r, g, b, a );
	SDL_RenderClear( renderer );
}

void ClearRender(SDL_Renderer* renderer, SDL_Color color){
	SDL_SetRenderDrawColor( renderer, color.r, color.g, color.b, color.a );
	SDL_RenderClear( renderer );
}

void SetRenderColor(SDL_Renderer* renderer, SDL_Color color){
	SDL_SetRenderDrawColor( renderer, color.r, color.g, color.b, color.a );
}

void FillRect(SDL_Renderer* renderer, SDL_Rect rect, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	SDL_SetRenderDrawColor( renderer, r, g, b, a );        
	SDL_RenderFillRect( renderer, &rect );
}

void FillRect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color){
	SDL_SetRenderDrawColor( renderer, color.r, color.g, color.b, color.a );        
	SDL_RenderFillRect( renderer, &rect );
}

void FillRect(SDL_Renderer* renderer, SDL_FRect rect, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	SDL_SetRenderDrawColor( renderer, r, g, b, a );        
	SDL_RenderFillRectF( renderer, &rect );
}

void FillRect(SDL_Renderer* renderer, SDL_FRect rect, SDL_Color color){
	SDL_SetRenderDrawColor( renderer, color.r, color.g, color.b, color.a );        
	SDL_RenderFillRectF( renderer, &rect );
}

void DrawRect(SDL_Renderer* renderer, SDL_Rect rect, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	SDL_SetRenderDrawColor( renderer, r, g, b, a );        
	SDL_RenderDrawRect( renderer, &rect );
}

void DrawRect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color){
	SDL_SetRenderDrawColor( renderer, color.r, color.g, color.b, color.a );        
	SDL_RenderDrawRect( renderer, &rect );
}

void DrawLine(SDL_Renderer* renderer, SDL_Point startPoint, SDL_Point endPoint, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	SDL_SetRenderDrawColor( renderer, r, g, b, a );        
	SDL_RenderDrawLine( renderer, startPoint.x, startPoint.y, endPoint.x, endPoint.y);
}

void DrawLine(SDL_Renderer* renderer, SDL_Point startPoint, SDL_Point endPoint, SDL_Color color){
	SDL_SetRenderDrawColor( renderer, color.r, color.g, color.b, color.a );        
	SDL_RenderDrawLine( renderer, startPoint.x, startPoint.y, endPoint.x, endPoint.y);
}

void DrawPoint(SDL_Renderer* renderer, SDL_Point point, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	SDL_SetRenderDrawColor( renderer, r, g, b, a );        
	SDL_RenderDrawPoint( renderer, point.x, point.y);
}

void DrawPoint(SDL_Renderer* renderer, SDL_Point point, const SDL_Color &color){
	SDL_SetRenderDrawColor( renderer, color.r, color.g, color.b, color.a );           
	SDL_RenderDrawPoint( renderer, point.x, point.y);
}

void SetRenderViewport(SDL_Renderer* renderer, SDL_Rect* viewport){
	SDL_RenderSetViewport( renderer, viewport);
}

void GetMouseCoordinates(float &x, float &y){
	int mX = 0, mY = 0;
	SDL_GetMouseState(&mX, &mY);
	x = mX; y = mY;
}

float Distance(SDL_Point &point1, SDL_Point &point2){
	return sqrt(pow(point1.x-point2.x, 2) + pow(point1.y - point2.y, 2));
}

float Distance(SDL_FPoint &point1, SDL_FPoint &point2){
	return sqrt(pow(point1.x-point2.x, 2) + pow(point1.y - point2.y, 2));
}

float SquaredDistance(SDL_Point &point1, SDL_Point &point2){
	return pow(point1.x-point2.x, 2) + pow(point1.y - point2.y, 2);
}

float SquaredDistance(SDL_FPoint &point1, SDL_FPoint &point2){
	return pow(point1.x-point2.x, 2) + pow(point1.y - point2.y, 2);
}

float AngleBetween(SDL_Point &vector1, SDL_Point &vector2){
	float radiansBetweem = std::abs(atan2f((float)vector1.y, (float)vector1.x) - atan2f((float)vector2.y, (float)vector2.x));
	return radiansBetweem * 180.0f / M_PI;
}

float AngleBetween(SDL_FPoint &vector1, SDL_FPoint &vector2){
	float radiansBetweem = std::abs(atan2f(vector1.y, vector1.x) - atan2f(vector2.y, vector2.x));
	return radiansBetweem * 180.0f / M_PI;
}

SDL_Point GetClosestPoint(SDL_Point &targetPoint, SDL_Rect &spaceRect){
	return SDL_Point{std::clamp(targetPoint.x, spaceRect.x, spaceRect.x + spaceRect.w), std::clamp(targetPoint.y, spaceRect.y, spaceRect.y + spaceRect.h)};
}
SDL_FPoint GetClosestPoint(SDL_FPoint &targetPoint, SDL_FRect &spaceRect){
	return SDL_FPoint{std::clamp(targetPoint.x, spaceRect.x, spaceRect.x + spaceRect.w), std::clamp(targetPoint.y, spaceRect.y, spaceRect.y + spaceRect.h)};
}

void CenterIn(SDL_Rect &dimensions, SDL_Rect newCenter){
	dimensions.x = newCenter.x + (newCenter.w-dimensions.w)>>1;
	dimensions.y = newCenter.y + (newCenter.h-dimensions.h)>>1;
}
void CenterIn(SDL_FRect &dimensions, SDL_FRect newCenter){
	dimensions.x = newCenter.x + (newCenter.w-dimensions.w)*0.5f;
	dimensions.y = newCenter.y + (newCenter.h-dimensions.h)*0.5f;
}

SDL_FPoint GetCenterOf(SDL_FRect &frect){
	return SDL_FPoint{.x = frect.x + frect.w * 0.5f, .y = frect.y + frect.h * 0.5f};
}

void ScaleCentered(SDL_FRect &toModify, float scale){
	toModify.x += toModify.w*0.5f;
	toModify.w *= scale;
	toModify.x -= toModify.w*0.5f;
	toModify.y += toModify.h*0.5f;
	toModify.h *= scale;
	toModify.y -= toModify.h*0.5f;
}

void ResizeCentered(SDL_FRect &toModify, float newW, float newH){
	toModify.x += toModify.w*0.5f;
	toModify.w = newW;
	toModify.x -= toModify.w*0.5f;
	toModify.y += toModify.h*0.5f;
	toModify.h = newH;
	toModify.y -= toModify.h*0.5f;
}

bool OverlapCircles(SDL_FPoint &center1, float radius1, SDL_FPoint &center2, float radius2){
	return SquaredDistance(center1, center2) <= (radius1*radius1+2.0f*radius1*radius2+radius2*radius2);
}

bool FRectInsideFRect(SDL_FRect &insider, SDL_FRect &outsider){
	return  insider.x > outsider.x &&
			insider.x + insider.w < outsider.x + outsider.w &&
			insider.y > outsider.y &&
			insider.y + insider.h < outsider.y + outsider.h;
}

bool FRectOutsideFRect(SDL_FRect &rect1, SDL_FRect &rect2){
	return  rect1.x > rect2.x + rect2.w ||
			rect1.x + rect1.w < rect2.x ||
			rect1.y > rect2.y + rect2.h ||
			rect1.y + rect1.h < rect2.y;
}

std::vector<SDL_Point> GetPointsInSegment(SDL_Point initialPoint, SDL_Point finalPoint){
	std::vector<SDL_Point> result;

	int xDif = std::abs(initialPoint.x-finalPoint.x), yDif = std::abs(initialPoint.y-finalPoint.y);

	if(xDif > yDif){
		result.reserve(xDif);

		float yGrowth = (finalPoint.y-initialPoint.y)/(float)xDif;
		float y = (float)initialPoint.y;

		if(initialPoint.x < finalPoint.x){
			for(int x = initialPoint.x; x <= finalPoint.x; ++x){
				result.emplace_back(x, (int)y);

				y += yGrowth;
			}
		} else {
			for(int x = initialPoint.x; x >= finalPoint.x; --x){
				result.emplace_back(x, (int)y);

				y += yGrowth;
			}
		}

	} else {
		result.reserve(yDif);
		float xGrowth = (finalPoint.x-initialPoint.x)/(float)yDif;
		float x = (float)initialPoint.x;
		if(initialPoint.y < finalPoint.y){
			for(int y = initialPoint.y; y <= finalPoint.y; ++y){
				result.emplace_back((int)x, y);

				x += xGrowth;
			}
		} else {
			for(int y = initialPoint.y; y >= finalPoint.y; --y){
				result.emplace_back((int)x, y);

				x += xGrowth;
			}
		}
	}

	return result;
}

bool ArePointsEqual(const SDL_Point &point1, const SDL_Point &point2){
	return point1.x == point2.x && point1.y == point2.y;
}

bool MakeEventRelativeToRect(const SDL_Rect &dimensions, int &eventX, int &eventY, SDL_Point &originalMousePosition, bool resetIfUnable){
	//First we set the mouse original position
	originalMousePosition.x = eventX;
	originalMousePosition.y = eventY;

	//Then we change the coordinates and check if it falls inside the window
	eventX -= dimensions.x;
	eventY -= dimensions.y;

	if(eventX >= 0 && eventX < dimensions.w && eventY >= 0 && eventY < dimensions.h){
		return false;
	}

	//If it doesn't we set the values back and return true, as it falls outside the rect
	if(resetIfUnable){
		eventX = originalMousePosition.x;
		eventY = originalMousePosition.y;
	}
	return true;
}

bool MakeEventRelativeToRect(const SDL_Rect &dimensions, int &eventX, int &eventY, SDL_Point &originalMousePosition, int *&pEventX, int *&pEventY, bool resetIfUnable){
	//First we set the mouse original position and the pointers 
	originalMousePosition.x = eventX;
	originalMousePosition.y = eventY;
	
	pEventX = &eventX;
	pEventY = &eventY;

	//Then we change the coordinates and check if it falls inside the window
	eventX -= dimensions.x;
	eventY -= dimensions.y;
	
	if(eventX >= 0 && eventX < dimensions.w && eventY >= 0 && eventY < dimensions.h){
		return false;
	}

	//If it doesn't we set the values back and return true, as it falls outside the rect
	if(resetIfUnable){
		eventX = originalMousePosition.x;
		eventY = originalMousePosition.y;
	}
	return true;
}