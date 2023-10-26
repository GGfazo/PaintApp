#pragma once
#include "SDL.h"
#include "SDL_ttf.h"
#include <string>
#include <memory>
#include <vector>
#include <span>
#include <algorithm>
#include <optional>

//For smart pointers usage
struct PointerDeleter{
    void operator()(SDL_Window *pWindow) const{ SDL_DestroyWindow(pWindow); }
    void operator()(SDL_Renderer *pRenderer) const{ SDL_DestroyRenderer(pRenderer); }
    void operator()(SDL_Texture *pTexture) const{ SDL_DestroyTexture(pTexture); }
    void operator()(SDL_Surface *pSurface) const{ SDL_FreeSurface(pSurface); }
    void operator()(TTF_Font *pFont) const{ TTF_CloseFont(pFont); }
};

class ConstantText{
    public:

    //Maybe add option for color
    ConstantText(const char *pText, std::shared_ptr<TTF_Font> pFont);

    void Reset(const char *pText, std::shared_ptr<TTF_Font> pFont);

    void SetX(float x);
    void SetY(float y);
    void SetWidth(float nWidth);
    void SetHeight(float nHeight);
    float GetWidth();
    SDL_Rect GetDimensions();

    void Draw(SDL_Renderer *pRenderer);

    private:

    bool mUpdateText = false;
    std::string mpActualText;
    std::shared_ptr<TTF_Font> mpFont;
    std::unique_ptr<SDL_Texture, PointerDeleter> mpTextTexture;
    SDL_Point mTextSize = {0, 0};
    SDL_Rect mDimensions = {0, 0, 0, 0};
};

//Used to manage the start and stop of text input as, if another textfield gets selected, the previously selected one may not handle that event and still be selected 
class TextInputManager{
    public:

    static void HandleEvent(SDL_Event *event){if(event->type == SDL_MOUSEBUTTONDOWN){UnsetRequester(mRequester);}}

    static void SetRequester(void *nRequester){mRequester = nRequester; SDL_StartTextInput();}
    static bool IsRequester(void *nRequester){return mRequester == nRequester;}
    static void UnsetRequester(void *nRequester){if(IsRequester(nRequester)) {mRequester = nullptr; SDL_StopTextInput();}}

    private:
    
    static void* mRequester;
};

class TextField{
    public:

    enum class TextFormat{
        HEX,
        WHOLE_POSITIVE,
        NONE
    };

    SDL_Rect dimensions;

    bool displayBackground = true;

    TextField(std::shared_ptr<TTF_Font> npFont, TextFormat nTextFormat, const std::string& nBlankText = "");
    ~TextField();

    //Sets mTextString to the given string and updates the texture. Checks for format compability (wether it is hex, whole...)
    void SetText(std::string_view nTextString);
    void SetBlankText(std::string_view nBlankText);
    void SetTextFormat(TextFormat nTextFormat);

    void SetColor(SDL_Color nTextColor);

    std::string_view GetText();
    bool HasChanged();

    bool HandleEvent(SDL_Event *event);
    void Update(float deltaTime);
    void Draw(SDL_Renderer *pRenderer);

    //Currently supports both hex and whole positive
    bool IsValidNumber();
    int GetAsNumber(bool *isValidData);

    //Currently only supports 6-byte hex color
    bool IsValidColor();
    SDL_Color GetAsColor(bool *isValidData);
    
    static bool IsValidColor(std::string text);
    static SDL_Color GetAsColor(std::string text, bool *isValidData);

    private:

    TextFormat mTextFormat;
    
    //Contains the text that gets displayed when the TextField is empty
    std::string mBlankText;

    std::string mTextString;
    std::shared_ptr<TTF_Font> mpFont;
    bool mSelected = false;
    bool mUpdateText = true;
    std::unique_ptr<SDL_Texture, PointerDeleter> mpTextTexture;

    SDL_Color mTextColor = {0, 0, 0, SDL_ALPHA_OPAQUE};
};

//A button that, when clicked, its internal value switches from true to false and viceversa
class TickButton{
    public:
    
    //The place where the button is rendered
    SDL_Rect dimensions;

    TickButton(SDL_Rect nDimensions, bool nValue = false);

    //returns true if clicked
    bool HandleEvent(SDL_Event *event);

    void Draw(SDL_Renderer *pRenderer);

    void SetValue(bool nValue);
    bool GetValue();

    private:

    //By default false, although it's value can be set on the constructor
    bool mValue = false;
};

class Slider{
    public:

    Slider(std::shared_ptr<TTF_Font> npFont, SDL_Rect nDimensions, float initialValue, float nMin = 0.0f, float nMax = 100.0f);

    void SetWidth(int nWidth);

    void SetDimensions(SDL_Rect nDimensions);
    SDL_Rect GetDimensions();

    void SetValue(float nValue);
    void SetMinValue(float nMin);
    void SetMaxValue(float nMax);
    float GetValue();

    //Returns true if the last SetValue gave a new value to mValue (aka nValue != mValue)
    bool HasChanged();

    bool HandleEvent(SDL_Event *event);
    void Draw(SDL_Renderer *pRenderer);
    
    private:

    SDL_Rect mDimensions;
    SDL_Rect mFilledDimensions;
    float mValue = 0.0f;
    float mMin = 0.0f, mMax = 0.0f;
    bool mSelected = false, mHasChanged = true;

    //TODO: make it so that you can set it (either via SetDecimalPlaces/SetPrecission or in the constructor)
    //It is only used if mValue is a floating point
    const int VALUE_DECIMAL_PLACES = 1;

    //Currently this textfield cannot be used to set the value
    TextField mTextField;
};

class ChoicesArray{
    public:

    ChoicesArray(SDL_Rect nDimensions, int nButtonsSize);
    
    void AddOption(std::string texturePath);
    void SetDimensions(SDL_Rect nDimensions);
    SDL_Rect GetDimensions();

    //Returns false if it wasn't able to set mLastChosen
    bool SetLastChosenOption(int nLastChosen);
    int GetLastChosenOption();

    //Only sets mLastChosen, doesn't have any effect on the textures nor does it check for it to be in range
    void UncheckedSetLastChosenOption(int nLastChosen);

    bool HandleEvent(SDL_Event *event);
    void Draw(SDL_Renderer *pRenderer);

    private:

    SDL_Rect mDimensions; //The space that can be used by the buttons
    int mButtonsSize; //The width and height of every button
    std::vector<std::unique_ptr<SDL_Texture, PointerDeleter>> mTextures;
    
    int mLastChosen = 0;

    bool mUpdateTextures = false; //If true, when Draw gets called, mTextures gets cleared and loaded with the ones from mTexturesPath 
    std::vector<std::string> mTexturesPaths;

    void UpdateTextures(SDL_Renderer *pRenderer);
};

class PositionPickerButton{
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
};

inline SDL_Point GetPointCell(SDL_Point originalPoint, float cellSize){
    return {(int)(originalPoint.x/cellSize), (int)(originalPoint.y/cellSize)};
}

struct Pencil{
    //Determines general way to draw pixels
    enum class PencilType{
        HARD, //There's no transparency
        SOFT  //The transparency of each pixel increases the farther they are from the center
    } pencilType = PencilType::SOFT;

    //'hardness' can have values in the range [0, 1]. Only has effect when 'pencilType' is set to soft
    //It determines the amount of pixels that get affected by the alpha change
    float hardness = 0.3f;

    //Determines how alpha values are calculated in soft pencils
    enum class AlphaCalculation{
        LINEAR = 0,     //The alpha value decreases linealy       (e.g: alpha = 1-distance/radius)
        QUADRATIC,  //The alpha value decreases quadratically (e.g: alpha = 1-pow(distance/radius, 2))
        EXPONENTIAL //The alpha value decreases exponentially (e.g: alpha = exp(e, -k*(distance/radius)), where k is a constant value)
    } alphaCalculation = AlphaCalculation::LINEAR;

    struct DrawPoint{
        SDL_Point pos = {0, 0};
        Uint8 alpha = 0;
    };

    int GetRadius();
    void SetRadius(int nRadius);
    std::vector<DrawPoint> ApplyOn(SDL_Point pixel, SDL_Rect *usedArea);

    void DrawPreview(SDL_Point center, float resolution, SDL_Renderer *pRenderer);

    private:

    //Can have values in the range [1,oo), but is always 1 less than what it's returned in GetRadius and the value of 'nRadius' (if valid) in SetRadius
	//Always set it to 1 less so that a radius set to 1 covers 1 pixel, while making it impossible to get returned a radius of 0
    int mRadius = 1;

    void FillHorizontalLine(int y, int minX, int maxX, const SDL_Point &circleCenter, std::vector<DrawPoint> &points);
};

class PencilModifier{
    public:

    //A SDL_Rect is used instead of a SDL_FRect as the floating precission is not needed and there's support for SDL_Rect as a viewport 
    SDL_Rect dimensions;

    PencilModifier(Pencil &nModifyPencil, SDL_Rect nDimensions);
    void SetModifiedPencil(Pencil &nModifyPencil);

    bool HandleEvent(SDL_Event *event);
    void Update(float deltaTime);
    void Draw(SDL_Renderer* pRenderer);

    private:
    
    //The pencil that gets the changes applied
    Pencil &mModifyPencil;

    //std::unique_ptr<TextField> mpPencilColor;
};

class MutableTexture{
    public:

    MutableTexture(SDL_Renderer *pRenderer, int width, int height, SDL_Color fillColor = {255, 255, 255});
    MutableTexture(SDL_Renderer *pRenderer, const char *pImage);

    SDL_Color GetPixelColor(SDL_Point pixel);

    void Clear(const SDL_Color &clearColor);

    //SetPixel methods. Both SetDrawnPixels blend the alpha of the pixel instead of just setting it
    void SetPixel(SDL_Point pixel, const SDL_Color &color);
    void SetPixels(std::span<SDL_Point> pixels, const SDL_Color &color);
    void SetDrawnPixels(std::span<Pencil::DrawPoint> pixels, const SDL_Color &color); 
    void SetPixelUnsafe(SDL_Point pixel, const SDL_Color &color);
    void SetPixelsUnsafe(std::span<SDL_Point> pixels, const SDL_Color &color);
    void SetDrawnPixelsUnsafe(std::span<Pencil::DrawPoint> pixels, const SDL_Color &color);

    //Updates the texture, applying all the changes made since the last call. Must be called outside the class
    void UpdateTexture();

    void DrawIntoRenderer(SDL_Renderer *pRenderer, const SDL_Rect &dimensions);

    void Save(const char *pSavePath);

    int GetWidth();
    int GetHeight();

    private:

    //This is what actually gets drawn into the screen
    std::unique_ptr<SDL_Texture, PointerDeleter> mpTexture;

    //This is what stores the pixel data
    std::unique_ptr<SDL_Surface, PointerDeleter> mpSurface;

    //Holds the position of all the pixels that have been modified since the last call to UpdateTexture
    std::vector<SDL_Point> mChangedPixels;

    //Used only in the constructor
    void UpdateWholeTexture();

    //Calculates the smallest rect that contains all points in 'mChangedPixels'
    SDL_Rect GetChangesRect();

    inline bool IsPixelOutsideImage(SDL_Point pixel){
        return (std::clamp(pixel.x, 0, mpSurface->w-1) != pixel.x) || (std::clamp(pixel.y, 0, mpSurface->h-1) != pixel.y);
    }
    inline Uint32* UnsafeGetPixel(SDL_Point index){
        return (Uint32*)((Uint8*)mpSurface->pixels + index.y * mpSurface->pitch + index.x * mpSurface->format->BytesPerPixel);
    }
};


class Canvas{
    public:

    Pencil pencil;

    bool saveOnDestroy = true;
    SDL_Rect viewport;
    //The color that the rest of the canvas (not the image) has, therefore it has no impact on the end result
    SDL_Color backgroundColor = {120, 120, 120};

    Canvas(SDL_Renderer *pRenderer, int nWidth, int nHeight);
    Canvas(SDL_Renderer *pRenderer, const char *pLoadFile);

    ~Canvas();

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
    
    std::string mSavePath;
};