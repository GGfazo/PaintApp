#pragma once

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include <string>
#include <vector>

class RenderedWindow{
    int windowsWidth;
    int windowsHeight;

    public:
    
    SDL_Window* pWindow;
    SDL_Renderer* pRenderer;

    //RenderedWindow() = default;
    RenderedWindow(int width, int height, Uint32 flags = 0, const char* windowsName = "Default name", int x = SDL_WINDOWPOS_UNDEFINED, int y = SDL_WINDOWPOS_UNDEFINED);
    ~RenderedWindow();
    void UpdateInternalDimensions();
    void GetWindowDimensions(int &width, int &height);
    int GetWindowWidth();
    int GetWindowHeight();
};

enum class Format
{
    BMP,
    PNG,
    TOTAL_FORMATS
};

//Retrieves the size of a png formated image
SDL_Point GetSizeOfPNG(const char *path);

//Frees the window and the renderer and stops dependenies, used by default in main.cpp 
void Release(SDL_Window *&window, SDL_Renderer *&renderer);

SDL_Surface *LoadSurface(const char *path, SDL_Surface *windowSurface, Format format = Format::PNG);

//Returns a pointer to a texture from the given path
SDL_Texture* LoadTexture(const char *path, SDL_Renderer *renderer, Format format = Format::PNG);

//Returns a pointer to a texture from the given path with a key color (for alpha)
SDL_Texture* LoadTexture(const char *path, SDL_Renderer *renderer, SDL_Color keyColor, Format format = Format::PNG);

//Returns a pointer to a texture from the given path without needing format (NF = No Format)
SDL_Texture* NFLoadTexture(std::string path, SDL_Renderer *renderer);

//Returns a pointer to a texture from the given path with a key color (for alpha) without needing format (NF = No Format)
SDL_Texture* NFLoadTexture(std::string path, SDL_Renderer *renderer, SDL_Color keyColor);

//Returns a pointer to a texture of the given text
SDL_Texture* LoadTextureFromText(const char *text, SDL_Renderer *renderer, TTF_Font *font, SDL_Color colorText = {0, 0, 0, SDL_ALPHA_OPAQUE});

//Returns a pointer to a font on the given path 
TTF_Font* LoadFont(std::string path, int size);

//Displays the given texture into the screen
void DisplayTexture(SDL_Renderer *renderer, SDL_Texture *texture);

//Displays the given texture into the screen at the marked position
void DisplayTexture(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *renderingPart);

//Modifies 'width' and 'height' by the scale times size of texture 
void GetTextureSize(SDL_Texture *texture, int *width, int *height, float scale = 1.0f);

//Modifies 'width' and 'height' by the scale times size of texture 
void GetTextureSize(SDL_Texture *texture, unsigned int *width, unsigned int *height, float scale = 1.0f);

//Modifies 'width' and 'height' by the scale times size of texture 
void GetTextureSize(SDL_Texture *texture, float *width, float *height, float scale = 1.0f);

//Fills the renderer with the given red, green, blue and alpha values
void ClearRender(SDL_Renderer* renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a = SDL_ALPHA_OPAQUE);

//Fills the renderer with the given color
void ClearRender(SDL_Renderer* renderer, SDL_Color color);

//Sets the render drawing color
void SetRenderColor(SDL_Renderer* renderer, SDL_Color color);

//Fills the designated rect with the given red, green, blue and alpha values
void FillRect(SDL_Renderer* renderer, SDL_Rect rect, uint8_t r, uint8_t g, uint8_t b, uint8_t a = SDL_ALPHA_OPAQUE);

//Fills the designated rect with the given color
void FillRect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color);

//Fills the designated frect with the given red, green, blue and alpha values
void FillRect(SDL_Renderer* renderer, SDL_FRect rect, uint8_t r, uint8_t g, uint8_t b, uint8_t a = SDL_ALPHA_OPAQUE);

//Fills the designated frect with the given color
void FillRect(SDL_Renderer* renderer, SDL_FRect rect, SDL_Color color);

//Draws (without filling) the designated rect with the given red, green, blue and alpha values
void DrawRect(SDL_Renderer* renderer, SDL_Rect rect, uint8_t r, uint8_t g, uint8_t b, uint8_t a = SDL_ALPHA_OPAQUE);

//Draws (without filling) the designated rect with the given color
void DrawRect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color);

//Draws a line from 'startPoint' to 'endPoint' with the given red, green, blue and alpha values
void DrawLine(SDL_Renderer* renderer, SDL_Point startPoint, SDL_Point endPoint, uint8_t r, uint8_t g, uint8_t b, uint8_t a = SDL_ALPHA_OPAQUE);

//Draws a line from 'startPoint' to 'endPoint' with the given color
void DrawLine(SDL_Renderer* renderer, SDL_Point startPoint, SDL_Point endPoint, SDL_Color color);

//Draws a point  with the given red, green, blue and alpha values
void DrawPoint(SDL_Renderer* renderer, SDL_Point point, uint8_t r, uint8_t g, uint8_t b, uint8_t a = SDL_ALPHA_OPAQUE);

//Draws a point  with the given color
void DrawPoint(SDL_Renderer* renderer, SDL_Point point, const SDL_Color &color);

//Sets the renderer viwport to the chosen one
void SetRenderViewport(SDL_Renderer* renderer, SDL_Rect* viewport);

void GetMouseCoordinates(float &x, float &y);

float Distance(SDL_Point &point1, SDL_Point &point2);
float Distance(SDL_FPoint &point1, SDL_FPoint &point2);
float SquaredDistance(SDL_Point &point1, SDL_Point &point2);
float SquaredDistance(SDL_FPoint &point1, SDL_FPoint &point2);

//Calculates the (smallest) angle between two vectors 
float AngleBetween(SDL_Point &vector1, SDL_Point &vector2);
float AngleBetween(SDL_FPoint &vector1, SDL_FPoint &vector2);

//Calculates the closest point inside 'spaceRect' to the target point
SDL_Point GetClosestPoint(SDL_Point &targetPoint, SDL_Rect &spaceRect);
SDL_FPoint GetClosestPoint(SDL_FPoint &targetPoint, SDL_FRect &spaceRect);

//Changes dimensions' x and y values so that the instance is centered in 'newCenter'
void CenterIn(SDL_Rect &dimensions, SDL_Rect newCenter);
void CenterIn(SDL_FRect &dimensions, SDL_FRect newCenter);

SDL_FPoint GetCenterOf(SDL_FRect &frect);

void ScaleCentered(SDL_FRect &toModify, float scale);
void ResizeCentered(SDL_FRect &toModify, float newW, float newH);

bool OverlapCircles(SDL_FPoint &center1, float radius1, SDL_FPoint &center2, float radius2);
bool FRectInsideFRect(SDL_FRect &insider, SDL_FRect &outsider);
bool FRectOutsideFRect(SDL_FRect &rect1, SDL_FRect &rect2);

//Returns a series of points, that belong in the segment defined by the passed points.
//The points returned have a unique x or y coordinate each (depending on the segment's slope)
std::vector<SDL_Point> GetPointsInSegment(SDL_Point initialPoint, SDL_Point finalPoint); 

//Returns true only if both x values and y values are equal (e.g. (10,12) == (10,12) => true)
bool operator==(const SDL_Point &point1, const SDL_Point &point2);

inline bool IsRectCompletelyInsideRect(const SDL_Rect &contained, const SDL_Rect &mainRect){
    return (mainRect.x <= contained.x) && (mainRect.y <= contained.y) &&
            ((mainRect.x + mainRect.w) >= (contained.x + contained.w)) && 
            ((mainRect.y + mainRect.h) >= (contained.y + contained.h));
}