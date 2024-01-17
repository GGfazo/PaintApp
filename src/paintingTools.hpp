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
#include <functional>
#include <type_traits>
#include <array>

class MutableTexture;
class Canvas;

//Given a file path, returns its contents as a std::string
std::string ReadFileToString(const std::string &path);

//We have to use floor, as just changing it into an int would drop the decimal place, making negative numbers erroneous
inline SDL_Point GetPointCell(SDL_Point originalPoint, float cellSize){
    return {(int)std::floor(originalPoint.x/cellSize), (int)std::floor(originalPoint.y/cellSize)};
}

//Use when sub pixels are required
inline SDL_FPoint GetRealPointCell(SDL_Point originalPoint, float cellSize){
    return {originalPoint.x/cellSize, originalPoint.y/cellSize};
}

//Provides a general space where the currently used tool (if needed, e.g: Pencil, Eraser, Fader) can store data 
//and achieve common results (basically having the same preview rects or circle for the same radius)
namespace tool_circle_data{
    extern bool needsUpdate;
    extern int radius;
    extern std::function<Uint8(const SDL_Point& center, const SDL_Point& position)> alphaCalculation;

    //The alpha of 'circleColor' is not used
    extern SDL_Color backgroundColor, circleColor;
    
    extern float rectsResolution;

    //Remember that this resolution only affects the size of the rects and has no impact on the SDL_Surface
    void SetResolution(float nRectsResolution);

    SDL_Surface *GetCircleSurface();
    std::vector<SDL_Rect> &GetPreviewRects();
    void DrawPreview(SDL_Point center, SDL_Renderer *pRenderer, SDL_Color previewColor);

    void UpdateCirclePixels();
    void UpdatePreviewRects();

    namespace{
        //Holds the pixels that may be modified
        extern std::unique_ptr<SDL_Surface, PointerDeleter> mpCircleSurface;

        //We use rects instead of stand alone pixels, not only for efficiency but also for better displaying
        extern std::vector<SDL_Rect> mPreviewRects;
        
        void FillHorizontalLine(int y, int minX, int maxX, const SDL_Point &circleCenter);
    }
};

struct Pencil{
    //Determines general way to draw pixels
    enum class PencilType{
        HARD,   //There's no transparency
        SOFT    //The transparency of each pixel increases the farther they are from the center
    };

    //Determines how alpha values are calculated in soft pencils
    enum class AlphaCalculation{
        LINEAR = 0, //The alpha value decreases linealy       (e.g: alpha = 1-distance/radius)
        QUADRATIC,  //The alpha value decreases quadratically (e.g: alpha = 1-pow(distance/radius, 2))
        EXPONENTIAL //The alpha value decreases exponentially (e.g: alpha = exp(e, -k*(distance/radius)), where k is a constant value)
    };

    void Activate();

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
    //'mHardness' can have values in the range [0, 1]. Only has effect when 'mPencilType' is set to soft
    //It determines the amount of pixels that get affected by the alpha change
    float mHardness = 0.3f;

    AlphaCalculation mAlphaCalculation = AlphaCalculation::LINEAR;
    PencilType mPencilType = PencilType::SOFT;

    std::function<Uint8(const SDL_Point& center, const SDL_Point& position)> GetPixelAlphaCalculation();
};

struct Eraser{
    void Activate();

    void ApplyOn(const std::span<SDL_Point> circleCenters, SDL_Surface *pSurfaceToModify, SDL_Rect *pTotalUsedArea = nullptr);

    //Only affects the preview display, has no effect on the value of ApplyOn. Calls UpdatePreviewRects
    void SetResolution(float nResolution);

    void DrawPreview(SDL_Point center, SDL_Renderer *pRenderer, SDL_Color previewColor = {0, 0, 0, SDL_ALPHA_OPAQUE});
};

//Sets the currently used color as the one on the canvas
struct ColorPicker{
    void Activate();

    //TODO: perhaps add a radius so that you can grab a mesh of all the colors?

    void GrabColor(Canvas *pCanvas, MutableTexture *pTexture, SDL_Point pixel);

    //Only affects the preview display, has no effect on the value of ApplyOn.
    void SetResolution(float nResolution);

    void DrawPreview(SDL_Point center, SDL_Renderer *pRenderer, SDL_Color previewColor = {0, 0, 0, SDL_ALPHA_OPAQUE});
};

//Delimiters the currentl area that can be modified
struct AreaDelimiter{
    bool loopBack = true; //If true, the points wrap around

    void Activate();

    //Handles mouse related events
    bool HandleEvent(SDL_Event *event, SDL_FPoint mousePoint);
    
    //Erases the selected point
    void EraseSelected();

    //Adds a point after the selected
    void AddBeforeSelected();

    void Clear();

    //Only affects the preview display, has no effect on the value of ApplyOn.
    void SetResolution(float nResolution);

    std::vector<SDL_FPoint> GetPointsCopy();

    void DrawPreview(SDL_Point realOffset, SDL_Renderer *pRenderer, SDL_Color previewColor = {0, 0, 0, SDL_ALPHA_OPAQUE}); //Draws a preview of the delimited area, representing the points with rects
    void DrawArea(SDL_Point realOffset, SDL_Renderer *pRenderer, SDL_Color previewColor = {0, 0, 0, SDL_ALPHA_OPAQUE}); //Draws a representation of the delimited area with lines for the conextions

    private:

    std::vector<SDL_FPoint> mPoints;
    SDL_FPoint *mpSelectedPoint = nullptr;
    bool mPointHolded = false;

    //Returns the closest point to the target with its distance, or nullptr if such point is too far
    SDL_FPoint *GetPoint(SDL_FPoint &target);
};

class MutableTexture{
    public:

    MutableTexture(SDL_Renderer *pRenderer, int width, int height, SDL_Color fillColor = {255, 255, 255, SDL_ALPHA_OPAQUE});
    MutableTexture(SDL_Renderer *pRenderer, const char *pImage);

    void AddFileAsLayer(SDL_Renderer *pRenderer, const char *pImage, SDL_Point imageSize);

    SDL_Color GetPixelColor(SDL_Point pixel, bool *validValue = nullptr);

    void Clear(const SDL_Color &clearColor);

    //The renderer is needed in order that the SDL_Texture can also be resized
    void ResizeAllLayers(SDL_Renderer *pRenderer, SDL_Point nSize);

    static void ApplyColorToColor(SDL_Color &baseColor, const SDL_Color &appliedColor);
    static void ApplyColorToColor(FColor &baseColor, const FColor &appliedColor);

    //SetPixel methods. Both SetDrawnPixels blend the alpha of the pixel instead of just setting it
    void SetPixel(SDL_Point pixel, const SDL_Color &color);
    void SetPixels(std::span<SDL_Point> pixels, const SDL_Color &color);
    void SetPixelUnsafe(SDL_Point pixel, const SDL_Color &color);
    void SetPixelsUnsafe(std::span<SDL_Point> pixels, const SDL_Color &color);

    //If the surface is modified, the texture won't be modified unless specified with a call to 'UpdateTexture' with a specified rect
    //The chosen layer is clamped between 0 and the amount of layers minus 1
    SDL_Surface *GetSurfaceAtLayer(int layer);
    //If the surface is modified, the texture won't be modified unless specified with a call to 'UpdateTexture' with a specified rect
    SDL_Surface *GetCurrentSurface();

    //Updates the texture, applying all the changes made since the last call. Must be called outside the class
    void UpdateTexture();
    void UpdateTexture(const SDL_Rect &rect);

    void AddLayer();
    bool DeleteCurrentLayer(); //Returns false if unable

    void SetLayerVisibility(bool visible); //Shows or hides the current layer based on the value of 'visible'
    bool GetLayerVisibility(); //Returns wether the current layer is drawn or not
    void SetLayerAlpha(Uint8 alpha); //Sets the alpha mod of the current layer
    Uint8 GetLayerAlpha(); //Returns the alpha mod of the current layer

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
    std::vector<bool> mShowSurface;

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

    enum class Tool : int{
        DRAW_TOOL = 0,
        ERASE_TOOL = 1,
        COLOR_PICKER = 2,
        AREA_DELIMITER = 3
    };

    static int maxAmountOfUndoActionsSaved; //This is only used in Canvas creation

    SDL_Color toolPreviewMainColor = {0, 0, 0, SDL_ALPHA_OPAQUE};
    SDL_Color toolPreviewAlternateColor = {255, 255, 255, SDL_ALPHA_OPAQUE};

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
    void OpenFile(SDL_Renderer *pRenderer, const char *pLoadFile, SDL_Point imageSize);

    SDL_Color GetColor();
    void SetColor(SDL_Color nDrawColor);

    void SetRadius(int nRadius);
    int GetRadius();

    //Like SetPixel and SetPixels but it uses the pencil/eraser and changes the value of mLastMousePixel
    void DrawPixel(SDL_Point localPixel);
    void DrawPixels(const std::vector<SDL_Point> &localPixels);
    void Clear(std::optional<SDL_Color> clearColor = {});

    void SetSavePath(const char *nSavePath);
    void SetOffset(int offsetX, int offsetY);
    void SetResolution(float nResolution);
    void SetTool(Tool nUsedTool);
    
    //Uses the data inside 'mAreaDelimiter' with the 'mPencil'
    void ApplyAreaOutline();

    void AppendCommand(const std::string &nCommand);
    std::string GiveCommands();

    void Undo();
    void Redo();

    void HandleEvent(SDL_Event *event);

    void Update(float deltaTime);

    void DrawIntoRenderer(SDL_Renderer *pRenderer);

    void Save();

    void CenterInViewport();

    int GetResolution();
    SDL_Point GetImageSize();
    SDL_Point GetGlobalPosition();

    void AddLayer(); //Makes sure the canvas isn't being drawn on and calls AddLayer method on the MutableTexture and saves this change to the undo chain
    void DeleteCurrentLayer(); //Makes sure the canvas isn't being drawn on and calls DeleteCurrentLayer method on the MutableTexture and saves this change to the undo chain
    void SetLayer(int nLayer); //Makes sure the canvas isn't being drawn on and calls SetLayer method on the MutableTexture
    void SetLayerVisibility(bool visible); //Makes sure the canvas isn't being drawn on and makes the current layer visible if 'visible' is true, 
    void SetLayerAlpha(Uint8 alpha); //Makes sure the canvas isn't being drawn on and sets the alpha mod of the current layer
    MutableTexture *GetImage();

    template <typename T>
    constexpr T *GetTool(){
        switch(mUsedTool){
            case Tool::DRAW_TOOL:
                if constexpr (std::is_same<T, decltype(mPencil)>::value) return &mPencil;
                else return nullptr;
            case Tool::ERASE_TOOL:
                if constexpr (std::is_same<T, decltype(mEraser)>::value) return &mEraser;
                else return nullptr;
            case Tool::COLOR_PICKER:
                if constexpr (std::is_same<T, decltype(mColorPicker)>::value) return &mColorPicker;
                else return nullptr;
            case Tool::AREA_DELIMITER:
                if constexpr (std::is_same<T, decltype(mAreaDelimiter)>::value) return &mAreaDelimiter;
                else return nullptr;
            default:
                return nullptr;
        }
    }

    private:

    Pencil mPencil;
    Eraser mEraser;
    ColorPicker mColorPicker;
    AreaDelimiter mAreaDelimiter;

    SDL_Rect mDimensions;
    static constexpr float M_MIN_RESOLUTION = 0.01f, M_MAX_RESOLUTION = 100.0f;
    float mResolution = 1;
    std::unique_ptr<MutableTexture> mpImage;

    //Every time 'mInternalTimer' surpasses 'M_MAX_TIMER', it saves the current image
    static constexpr float M_MAX_TIMER = 300.0f; 
    float mInternalTimer = 0.0f;

    //Commands that will be executed by the AppManager
    std::string mCommands = "";

    SDL_Color mDrawColor = {255, 0, 0, SDL_ALPHA_OPAQUE};
    bool mHolded = false;
    SDL_Point mLastMousePixel;

    Tool mUsedTool = Tool::DRAW_TOOL;

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
        DisplayingHolder() = default;
        DisplayingHolder(Canvas *npOwner);
        void Update();

        static constexpr int MAX_BORDER = 20;
        SDL_Rect backgroundRects[4] {{-1,-1,-1,-1}, {-1,-1,-1,-1}, {-1,-1,-1,-1}, {-1,-1,-1,-1}};

        SDL_Rect squaresViewport = {-1,-1,-1,-1};
        std::vector<SDL_Rect> lightGreySquares{}, darkGreySquares{};
        const SDL_Color grey[2] = {SDL_Color{205, 205, 205}, SDL_Color{155, 155, 155}};

        private:

        Canvas *mpOwner;
    } mDisplayingHolder;

    class ActionsManager{
        public:

        enum class Action{
            NONE,
            STROKE,
            LAYER_CREATION,
            LAYER_DESTRUCTION
        };

        std::vector<SDL_Point> pointTracker; //Used externally to help calculate change area

        void Initialize(int nMaxUndoActions);

        void SetOriginalLayer(SDL_Surface *pSurfaceToCopy, int surfaceLayer);
        void SetChange(SDL_Rect affectedRegion, SDL_Surface *pResultingSurface);
        void ClearRedoData();
        void ClearData(); //Goes back to the initial state, except for the value of mMaxActionsAmount
        
        void SetLayerCreation(); //It's assumed that the layer created is the one passed to SetOriginalLayer
        void SetLayerDestruction(); //It's assumed that the layer destructed is the one passed to SetOriginalLayer

        int GetUndoLayer(); //Retrieves the layer where the undo (indicated by 'mActionIndex') is expected to be performed
        Action GetUndoType(); //Retrieves what action will be undone next
        bool UndoChange(SDL_Surface *pSurfaceToUndo, SDL_Rect *undoneRegion = nullptr); //Returns true if the change was undone
        
        int GetRedoLayer(); //Retrieves the layer where the redo (indicated by 'mActionIndex+1') is expected to be performed
        Action GetRedoType(); //Retrieves what action will be redone next
        bool RedoChange(SDL_Surface *pSurfaceToRedo, SDL_Rect *redoneRegion = nullptr); //Returns true if the change was redone

        private:

        struct LayeredRect{
            SDL_Rect rect;
            int layer;
        };

        int mMaxActionsAmount = -1;

        //Indicates the position of the last change recorded, or -1 if none
        int mActionIndex = -1;

        //Refers to the amount of actions set
        int mCurrentMaxIndex = -1;

        //Set to a copy of the original layer before applying a change that will be saved, so we can record the initial state
        std::unique_ptr<SDL_Surface, PointerDeleter> mpOriginalLayer;
        //We store the layer of the original surface for commodity. If the surface changes, this may also
        int mOriginalLayer = -1;

        //Despite all of these being vectors, their sizes should only be set in the method 'Initialize()'. Therefore, methods like push_back or emplace_back musn't be used
        std::vector<LayeredRect> mChangedRects;
        std::vector<std::unique_ptr<SDL_Surface, PointerDeleter>> mInitialSurface;
        std::vector<std::unique_ptr<SDL_Surface, PointerDeleter>> mEndingSurface;

        void RotateUndoHistoryIfFull();
    } mActionsManager;

    void UpdateRealPosition(){mRealPosition = {(float)mDimensions.x, (float)mDimensions.y};}
    void UpdateLayerOptions(); //Should be called when the current layer has been changed
};