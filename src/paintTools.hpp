#pragma once
#include "SDL.h"
#include "SDL_ttf.h"
#include <string>
#include <memory>
#include <vector>
#include <span>
#include <algorithm>
#include <optional>
#include <cmath>

//Destructors for smart pointers
struct PointerDeleter{
    void operator()(SDL_Window *pWindow) const{ SDL_DestroyWindow(pWindow); }
    void operator()(SDL_Renderer *pRenderer) const{ SDL_DestroyRenderer(pRenderer); }
    void operator()(SDL_Texture *pTexture) const{ SDL_DestroyTexture(pTexture); }
    void operator()(SDL_Surface *pSurface) const{ SDL_FreeSurface(pSurface); }
    void operator()(TTF_Font *pFont) const{ TTF_CloseFont(pFont); }
};

//A class that holds and displays a given text. It can be moved and its dimensions can be resized
class ConstantText{
    public:

    //The constructor, takes the text that is going to get displayed and the font that will be used to create the texture when Draw is called for the first time
    ConstantText(const char *pText, std::shared_ptr<TTF_Font> pFont);

    //Sets the internal text and font, and sets the flag that updates the texture in Draw
    void Reset(const char *pText, std::shared_ptr<TTF_Font> pFont);

    //The next set of methods, modify or return the coordinates and sizes of the text 
    void SetX(int x);
    void SetY(int y);
    //Setting the width or the height, accordingly sets the other using the internal text size so it doesn't look stetched
    void SetWidth(int nWidth);
    void SetHeight(int nHeight);
    int GetWidth();
    SDL_Rect GetDimensions();

    //Creates the texture, if needed, and displays the text into the renderer, on the set coordinates
    void Draw(SDL_Renderer *pRenderer);

    private:

    //When set to true, calling draw will (once) create a texture from the text using the font
    bool mUpdateText = false;
    //The text that gets displayed, in a string format. Used to allow the creation of the texture in Draw, instead of having to pass the renderer to the constructor / Reset method
    std::string mpActualText;
    //The font used in order to create the texture. Used to allow the creation of the texture in Draw, instead of having to pass the renderer to the constructor / Reset method
    std::shared_ptr<TTF_Font> mpFont;
    //The texture
    std::unique_ptr<SDL_Texture, PointerDeleter> mpTextTexture;
    //The actual width and height in pixels of the texture
    SDL_Point mTextSize = {0, 0};
    //The coordinates and sizes used for displaying the text
    SDL_Rect mDimensions = {0, 0, 0, 0};
};

//Used to manage the start and stop of text input as, if another textfield gets selected, the previously selected one may not handle that event and still be selected 
class TextInputManager{
    public:

    //This is called independantly, from the AppManager, in order to make sure that if a click occurs and no text field is affected by it, the click still results in the stop of text input
    static void HandleEvent(SDL_Event *event){if(event->type == SDL_MOUSEBUTTONDOWN){UnsetRequester(mRequester);}}

    //Sets the current entity that is requesting for the text input to be active 
    static void SetRequester(void *nRequester){mRequester = nRequester; SDL_StartTextInput();}
    //Verifies if the passed entity is the current requester 
    static bool IsRequester(void *nRequester){return mRequester == nRequester;}
    //Unsets the passed entity as the requester. If it actually was the requester, requester is unset and the text input gets stopped
    static void UnsetRequester(void *nRequester){if(IsRequester(nRequester)) {mRequester = nullptr; SDL_StopTextInput();}}

    private:
    
    //The entity that wants the text input to be active
    static void* mRequester;
};

//Represents a text field that gets displayed. The user can interact with it, via entering text, that can be retrieved by the app.
class TextField{
    public:

    //The text format determines what kind of input is allowed by the TextField and the means that the text can be retrieved as
    enum class TextFormat{
        HEX, //The text must have a length from 0 to 6, and only characters from 0 to 9 and a to f (upcase or downcase)
        WHOLE_POSITIVE, //The text must be composed by characters from 0 to 9
        NONE
    };

    //The coordinates and sizes used to display the text field
    SDL_Rect dimensions;

    //If true, in Draw, before drawing the text, two simple grey rects will be displayed as a background for the text 
    bool displayBackground = true;

    //The constructor takes a font, needed to create the texture; the text format, which will determine how the input is treated;
    //and an optional blank text that gets displayed when the user has deleted all input
    TextField(std::shared_ptr<TTF_Font> npFont, TextFormat nTextFormat, const std::string& nBlankText = "");
    ~TextField();

    //Sets mTextString to the given string and updates the texture. Checks for format compability (wether it is hex, whole...)
    //If the new text can be converted into a color by GetAsColor, then that color gets set
    void SetText(std::string_view nTextString);

    //Appends the given text, using the internal position of the cursor and updating it
    void AppendText(std::string_view toAppendText);

    //Removes a given amount of characters using the internal position of the cursor and updating it
    void RemoveCharacters(int charactersAmount = 1);

    //Sets the blank text that displays when there is no inputted info. Does not check for format compability
    void SetBlankText(std::string_view nBlankText);
    void SetTextFormat(TextFormat nTextFormat);

    //Sets the color of the text (only the inputed text, has no effect in the blank text)
    void SetColor(SDL_Color nTextColor);

    //Returns a string view to the inputted text
    std::string_view GetText();

    //Returns true if the function text texture is going to get updated the next time Draw is called
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
    
    //Performs the same operations as the non static methods, but on a specific string
    static bool IsValidColor(std::string text);
    static SDL_Color GetAsColor(std::string text, bool *isValidData);

    private:

    class Cursor{
        public:

            Cursor(int nPosition);

            void SetPotition(int nPosition);
            int GetPosition();
            void DecreasePosition(int minPosition, int maxPosition, int amount = 1);
            void IncreasePosition(int minPosition, int maxPosition, int amount = 1);

            void Draw(SDL_Renderer* pRenderer, SDL_Color cursorColor, int height, TTF_Font *pFont, const std::string& text);

        private:
        
            int mPosition;

    } mCursor = Cursor(0);

    //Determines the way input is handled and certain info is retrieved
    TextFormat mTextFormat;
    
    //Contains the text that gets displayed when the TextField is empty
    std::string mBlankText;

    //Contains the text currently inputed into the TextField
    std::string mTextString;

    //The font used to update the texture
    std::shared_ptr<TTF_Font> mpFont;
    
    //Used to handle text input
    bool mSelected = false;

    //When true, Draw updates the  mpTextTexture
    bool mUpdateText = true;

    //The texture of the text
    std::unique_ptr<SDL_Texture, PointerDeleter> mpTextTexture;

    //The color used to draw the inputted text
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

    //If mustUpdate is set to false, the text and filled dimensions won't get updated when the new value is the same as the current one
    void SetValue(float nValue, bool mustUpdate = false);
    void SetMinValue(float nMin);
    void SetMaxValue(float nMax);
    void SetDecimalPlaces(int nDecimal);
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
    int mDecimalPlaces = 1;

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

    struct DrawPoint{
        SDL_Point pos = {0, 0};
        Uint8 alpha = 0;
    };

    //SetRadius calls UpdatePreviewRects
    void SetRadius(int nRadius);
    int GetRadius();

    void SetHardness(float nHardness);
    float GetHardness();
    
    void SetAlphaCalculation(AlphaCalculation nAlphaCalculation);
    void SetPencilType(PencilType nPencilType);

        std::vector<SDL_Point> GetAffectedPixels(SDL_Point pixel, SDL_Rect *usedArea);
    std::vector<DrawPoint> ApplyOn(SDL_Point pixel, SDL_Rect *usedArea);

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

    //Holds the relative positions for the pixels that should be coloured with the current radius, and their alpha values
    std::vector<DrawPoint> mCirclePixels;

    //We use rects instead of stand alone pixels, not only for efficiency but also for better displaying
	std::vector<SDL_Rect> mPreviewRects;
    float mRectsResolution = 1.0f;

    void UpdateCirclePixels();
    void UpdateCircleAlphas();
    void SetPixelAlpha(const SDL_Point &center, DrawPoint &pixel);
    void FillHorizontalLine(int y, int minX, int maxX, const SDL_Point &circleCenter, std::vector<DrawPoint> &points);
    void UpdatePreviewRects();
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

    MutableTexture(SDL_Renderer *pRenderer, int width, int height, SDL_Color fillColor = {255, 255, 255, SDL_ALPHA_OPAQUE});
    MutableTexture(SDL_Renderer *pRenderer, const char *pImage);

    SDL_Color GetPixelColor(SDL_Point pixel, bool *validValue = nullptr);

    void Clear(const SDL_Color &clearColor);

    void ApplyColorToColor(SDL_Color &baseColor, const SDL_Color &appliedColor);

    //SetPixel methods. Both SetDrawnPixels blend the alpha of the pixel instead of just setting it
    void SetPixel(SDL_Point pixel, const SDL_Color &color);
    void SetPixels(std::span<SDL_Point> pixels, const SDL_Color &color);
    void SetDrawnPixels(std::span<Pencil::DrawPoint> pixels, const SDL_Color &color); 
    void SetPixelUnsafe(SDL_Point pixel, const SDL_Color &color);
    void SetPixelsUnsafe(std::span<SDL_Point> pixels, const SDL_Color &color);
    void SetDrawnPixelsUnsafe(std::span<Pencil::DrawPoint> pixels, const SDL_Color &color);

    //Updates the texture, applying all the changes made since the last call. Must be called outside the class
    void UpdateTexture();

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

    //This is what actually gets drawn into the screen
    std::vector<std::unique_ptr<SDL_Texture, PointerDeleter>> mTextures;

    //This is what stores the pixel data
    std::vector<std::unique_ptr<SDL_Surface, PointerDeleter>> mSurfaces;

    //Holds the position of all the pixels that have been modified since the last call to UpdateTexture
    std::vector<SDL_Point> mChangedPixels;

    //Used only in the constructor
    void UpdateWholeTexture();

    //Calculates the smallest rect that contains all points in 'mChangedPixels'
    SDL_Rect GetChangesRect();

    inline bool IsPixelOutsideImage(SDL_Point pixel){
        return (std::clamp(pixel.x, 0, mSurfaces[mSelectedLayer]->w-1) != pixel.x) || (std::clamp(pixel.y, 0, mSurfaces[mSelectedLayer]->h-1) != pixel.y);
    }
    inline Uint32* UnsafeGetPixel(SDL_Point index){
        return (Uint32*)((Uint8*)mSurfaces[mSelectedLayer]->pixels + index.y * mSurfaces[mSelectedLayer]->pitch + index.x * mSurfaces[mSelectedLayer]->format->BytesPerPixel);
    }
    inline Uint32* UnsafeGetPixel(SDL_Point index, int layer){
        return (Uint32*)((Uint8*)mSurfaces[layer]->pixels + index.y * mSurfaces[layer]->pitch + index.x * mSurfaces[layer]->format->BytesPerPixel);
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
    
    std::string mSavePath;
};