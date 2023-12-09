#pragma once
#include "SDL.h"
#include "SDL_ttf.h"
#include "renderLib.hpp"
#include <string>
#include <memory>
#include <vector>
#include <span>
#include <algorithm>
#include <optional>
#include <cmath>

/*class PositionPickerButton{
    public:

    SDL_Rect dimensions;

    PositionPickerButton(SDL_Texture *npTexture);

    SDL_Point GetPositionPicked(bool *wasSelected);

    bool HandleEvent(SDL_Event *event);
    void Draw(SDL_Renderer *pRenderer);

    private:

    enum class PickingState{
        NONE,
        BUTTON_CLICKED,
        POSITION_SELECTED
    };

    PickingState mPickingState = PickingState::NONE;
    SDL_Point mMouseClick;
    std::unique_ptr<SDL_Texture, PointerDeleter> mpTexture;
};*/

//We have to use floor, as just changing it into an int would drop the decimal place, making negative numbers erroneous
inline SDL_Point GetPointCell(SDL_Point originalPoint, float cellSize){
    return {(int)std::floor(originalPoint.x/cellSize), (int)std::floor(originalPoint.y/cellSize)};
}

struct Pencil{
    //Determines general way to draw pixels
    enum class PencilType{
        HARD, //There's no transparency
        SOFT  //The transparency of each pixel increases the farther they are from the center
    };

    //Determines how alpha values are calculated in soft pencils
    enum class AlphaCalculation{
        LINEAR = 0,     //The alpha value decreases linealy       (e.g: alpha = 1-distance/radius)
        QUADRATIC,  //The alpha value decreases quadratically (e.g: alpha = 1-pow(distance/radius, 2))
        EXPONENTIAL //The alpha value decreases exponentially (e.g: alpha = exp(e, -k*(distance/radius)), where k is a constant value)
    };

    //SetRadius calls UpdatePreviewRects
    void SetRadius(int nRadius);
    int GetRadius();

    void SetHardness(float nHardness);
    float GetHardness();
    
    void SetAlphaCalculation(AlphaCalculation nAlphaCalculation);
    void SetPencilType(PencilType nPencilType);

    //Applies the current pencil to the passed surface on the given centers
    void ApplyOn(const std::span<SDL_Point> circleCenters, SDL_Color drawColor, SDL_Surface *pSurfaceToModify, SDL_Rect *pTotalUsedArea = nullptr);

    //Only affects the preview display, has no effect on the value of ApplyOn. Calls UpdatePreviewRects
    void SetResolution(float nResolution);

    void DrawPreview(SDL_Point center, SDL_Renderer *pRenderer, SDL_Color previewColor = {0, 0, 0, SDL_ALPHA_OPAQUE});

    private:

    //Can have values in the range [1,oo), but is always 1 less than what it's returned in GetRadius and the value of 'nRadius' (if valid) in SetRadius
	//Always set it to 1 less so that a radius set to 1 covers 1 pixel, while making it impossible to get returned a radius of 0
    int mRadius = 1;

    //'mHardness' can have values in the range [0, 1]. Only has effect when 'mPencilType' is set to soft
    //It determines the amount of pixels that get affected by the alpha change
    float mHardness = 0.3f;

    AlphaCalculation mAlphaCalculation = AlphaCalculation::LINEAR;
    PencilType mPencilType = PencilType::SOFT;

    //Holds the pixels that should be coloured with the current radius, and their alpha values
    std::unique_ptr<SDL_Surface, PointerDeleter> mpCircleSurface;

    //We use rects instead of stand alone pixels, not only for efficiency but also for better displaying
	std::vector<SDL_Rect> mPreviewRects;
    float mRectsResolution = 1.0f;

    void UpdateCirclePixels();
    Uint8 GetPixelAlpha(const SDL_Point &center, const SDL_Point &pixel);
    void FillHorizontalLine(int y, int minX, int maxX, const SDL_Point &circleCenter, SDL_Surface *points);
    void UpdatePreviewRects();
};

class MutableTexture{
    public:

    MutableTexture(SDL_Renderer *pRenderer, int width, int height, SDL_Color fillColor = {255, 255, 255, SDL_ALPHA_OPAQUE});
    MutableTexture(SDL_Renderer *pRenderer, const char *pImage);

    SDL_Color GetPixelColor(SDL_Point pixel, bool *validValue = nullptr);

    void Clear(const SDL_Color &clearColor);

    static void ApplyColorToColor(SDL_Color &baseColor, const SDL_Color &appliedColor);
    static void ApplyColorToColor(FColor &baseColor, const FColor &appliedColor);

    //SetPixel methods. Both SetDrawnPixels blend the alpha of the pixel instead of just setting it
    void SetPixel(SDL_Point pixel, const SDL_Color &color);
    void SetPixels(std::span<SDL_Point> pixels, const SDL_Color &color);
    void SetPixelUnsafe(SDL_Point pixel, const SDL_Color &color);
    void SetPixelsUnsafe(std::span<SDL_Point> pixels, const SDL_Color &color);
    
    //If the surface is modified, the texture won't be modified unless specified with a call to 'UpdateTexture' with a specified rect
    SDL_Surface *GetCurrentSurface();

    //Updates the texture, applying all the changes made since the last call. Must be called outside the class
    void UpdateTexture();
    void UpdateTexture(const SDL_Rect &rect);

    void AddLayer(SDL_Renderer *pRenderer);
    void DeleteCurrentLayer();
    
    //Sets the selected layer to the chosen one clamped between 0 and the amount of layers minus 1
    void SetLayer(int nLayer);
    int GetLayer();
    int GetTotalLayers();

    void DrawIntoRenderer(SDL_Renderer *pRenderer, const SDL_Rect &dimensions);

    //Returns true if unable to save
    bool Save(const char *pSavePath);

    int GetWidth();
    int GetHeight();

    private:

    int mSelectedLayer = 0;

    //This is what stores the pixel data
    std::vector<std::unique_ptr<SDL_Surface, PointerDeleter>> mpSurfaces;

    //Formed by the compound of surfaces. It's what gets drawn into the screen
    std::unique_ptr<SDL_Texture, PointerDeleter> mpTexture;

    //Holds the position of all the pixels that have been modified since the last call to UpdateTexture
    std::vector<SDL_Point> mChangedPixels;

    //Used only in the constructor
    void UpdateWholeTexture();

    //Calculates the smallest rect that contains all points in 'mChangedPixels'
    SDL_Rect GetChangesRect();

    inline bool IsPixelOutsideImage(SDL_Point pixel){
        return (std::clamp(pixel.x, 0, mpSurfaces[mSelectedLayer]->w-1) != pixel.x) || (std::clamp(pixel.y, 0, mpSurfaces[mSelectedLayer]->h-1) != pixel.y);
    }
    inline Uint32* UnsafeGetPixel(SDL_Point index){
        return UnsafeGetPixelFromSurface<Uint32>(index, mpSurfaces[mSelectedLayer].get());
    }
    inline Uint32* UnsafeGetPixel(SDL_Point index, int layer){
        return UnsafeGetPixelFromSurface<Uint32>(index, mpSurfaces[layer].get());
    }
};

//TODO: add an actual base class Tool, that has method to process a quantity of pixels. The Pencil class would inherit from it, as so would Eraser, ColorPicker, RangeSelection  
class Canvas{
    public:

    enum class Tool{
        DRAW_TOOL = 0,
        ERASE_TOOL = 1
    } usedTool = Tool::ERASE_TOOL;

    Pencil pencil;
    SDL_Color pencilDisplayMainColor = {0, 0, 0, SDL_ALPHA_OPAQUE};
    SDL_Color pencilDisplayAlternateColor = {255, 255, 255, SDL_ALPHA_OPAQUE};

    float defaultMovementSpeed = 50.0f;
    float fastMovementSpeed = 150.0f;

    bool saveOnDestroy = true;
    SDL_Rect viewport;
    //The color that the rest of the canvas (not the image) has, therefore it has no impact on the end result
    SDL_Color backgroundColor = {120, 120, 120};

    Canvas(SDL_Renderer *pRenderer, int nWidth, int nHeight);
    Canvas(SDL_Renderer *pRenderer, const char *pLoadFile);

    ~Canvas();

    void Resize(SDL_Renderer *pRenderer, int nWidth, int nHeight);
    void OpenFile(SDL_Renderer *pRenderer, const char *pLoadFile);

    SDL_Color GetColor();
    void SetColor(SDL_Color nDrawColor);

    //Like SetPixel and SetPixels but it uses the pencil and changes the value of mLastMousePixel
    void DrawPixel(SDL_Point localPixel);
    void DrawPixels(const std::vector<SDL_Point> &localPixels);
    void Clear(std::optional<SDL_Color> clearColor = {});

    void SetSavePath(const char *nSavePath);
    void SetOffset(int offsetX, int offsetY);
    void SetResolution(float nResolution);

    void HandleEvent(SDL_Event *event);

    void Update(float deltaTime);

    void DrawIntoRenderer(SDL_Renderer *pRenderer);

    void Save();

    void CenterInViewport();

    int GetResolution();
    SDL_Point GetImageSize();
    SDL_Point GetGlobalPosition();
    MutableTexture *GetImage();

    private:

    SDL_Rect mDimensions;
    static constexpr float M_MIN_RESOLUTION = 0.01f, M_MAX_RESOLUTION = 100.0f;
    float mResolution = 1;
    std::unique_ptr<MutableTexture> mpImage;

    //Every time it reaches 'M_MAX_TIMER', it saves the current image
    static constexpr float M_MAX_TIMER = 30.0f; 
    float mInternalTimer = 0.0f;

    SDL_Color mDrawColor = {255, 0, 0, SDL_ALPHA_OPAQUE};
    bool mHolded = false;
    SDL_Point mLastMousePixel;

    enum Movement : unsigned int{
        NONE =  0b0000,
        LEFT =  0b0001,
        RIGHT = 0b0010,
        UP =    0b0100,
        DOWN =  0b1000
    };
    unsigned int mCanvasMovement = Movement::NONE;
    SDL_FPoint mRealPosition;
    
    std::string mSavePath;

    //Holds data used during the display of the canvas, so that it doesn't have to be calculated each time 'DrawIntoRenderer' is called
    //Should be updated each time 'mDimensions' changes
    struct DisplayingHolder{
        DisplayingHolder(Canvas *npOwner);
        void Update();

        static constexpr int MAX_BORDER = 20;
        SDL_Rect backgroundRects[4] {{-1,-1,-1,-1}, {-1,-1,-1,-1}, {-1,-1,-1,-1}, {-1,-1,-1,-1}};

        std::vector<SDL_Rect> lightGreySquares{}, darkGreySquares{};
        const SDL_Color grey[2] = {SDL_Color{205, 205, 205}, SDL_Color{155, 155, 155}};

        private:

        Canvas *mpOwner;
    } mDisplayingHolder;

    void UpdateRealPosition(){mRealPosition = {(float)mDimensions.x, (float)mDimensions.y};}
};