#pragma once
#include "options.hpp"
#include "paintingTools.hpp"
#include <array>
#include <span>
#include <functional>

class InternalWindow{
    public:

    struct InitializationData{
        //This name identifies the window
        std::string windowName;

        //The amount of options inside the optionsInfo string (for memory reserve purposses)
        int dataAmount;

        //Info from lines in the next format ('[]' are used for concepts and '()' for aclarations):
        //[ID(to know how to use the option)]_[Input Method (a single character to identify if a slider/text/tick/etc. is used)]_[extra info(literally)][\n]
        std::string_view optionsInfo;
    };

    InternalWindow(SDL_Point optionsSize, InitializationData nData, SDL_Renderer *pRenderer);

    //Sets the InternalWindow's position to the chosen x and y, only if this does not crop the InternalWindow
    void SetPosition(int x, int y);
    void SetSize(int w, int h);

    void SetDimensions(const SDL_Rect &nDimensions);

    void AddTemporalData(OptionInfo *newData);
    std::vector<std::shared_ptr<OptionInfo>> &GetTemporalData();

    std::vector<Option*> GetOptionsWithTag(Option::Tag identifier);

    const std::string_view GetName();

    //Either minimizes or un-minimizes the window, depending on its previous state. The window, when minimized, just displays a small logo in a square.
    void Minimize();

    bool HandleEvent(SDL_Event *event);
    void Update(float deltaTime);
    void Draw(SDL_Renderer *pRenderer);

    std::vector<std::shared_ptr<OptionInfo>> GetData();

    private:

    SDL_Rect mDimensions = {0, 0, 200, 200};
    int mInnerBorder = 4;
    int mMinGap = mInnerBorder*2+1;
    SDL_Rect mContentDimensions = {0, 0, 100, 100};

    //Identifies this window
    const std::string M_WINDOW_NAME;

    bool mMinimized = false;
    SDL_Point mPreMiniSize = {0, 0};
    std::unique_ptr<SDL_Texture, PointerDeleter> mpIcon;
    static constexpr SDL_Point M_MIN_SIZE = {14*3, 14*3};

    std::vector<std::unique_ptr<Option>> mOptions;

    //Set and read by the app manager, used for temporally saving special input from buttons (e.g: a button to choose the desired width for the new Canvas that hasn't been created yet)
    std::vector<std::shared_ptr<OptionInfo>> mTemporalData;

    bool mResizeOnDrag = false; //Set to true while left ctrl is held down
    enum class DraggedState{
        NONE,
        MOVEMENT,
        RESIZE_TOP,
        RESIZE_LEFT,
        RESIZE_BOTTOM,
        RESIZE_RIGHT
    };
    DraggedState mDraggedState = DraggedState::NONE; //Set to movement if the InternalWindow's border gets clicked while left ctrl is not being held down
    SDL_Rect mDraggedData = {0, 0, 0, 0}; //Set to the relative mouse position when the window's border was clicked, or to the window dimensions in case of resize

    enum class Border{
        NONE,
        TOP,
        LEFT,
        BOTTOM,
        RIGHT
    };

    //Returns the width dedicated to the text of each window's option
    int ProcessWindowInfo(std::string_view info);

    void UpdateContentDimensions();

    //Returns true if the given point falls exactly inside the inner border of the window
    //If border != nullptr, it gets assigned to the value of the exact border with the preferences being TOP > LEFT > BOTTOM > RIGHT
    bool PointInsideInnerBorder(SDL_Point point, Border *border);
    
    friend class AppManager;
};

class MainOption{
    public:

    MainOption() = default;
    MainOption(SDL_Rect nDimensions, const char* text);

    bool HandleEvent(SDL_Event *pEvent);
    void Draw(SDL_Renderer *pRenderer);

    private:
    
    SDL_Rect mDimensions;
    ConstantText mText;
};

class MainBar{
    public:

    enum class MainOptionIDs : int{
        SAVE = 0,
        CLEAR = 1,
        NEW_CANVAS = 2,
        PREFERENCES = 3
    };

    MainBar(SDL_Rect nDimensions);

    void SetWidth(int nWidth);

    bool HandleEvent(SDL_Event *pEvent);
    void Draw(SDL_Renderer *pRenderer);

    std::shared_ptr<OptionInfo> GetData();

    private:

    SDL_Rect mDimensions = {0, 0, 1920, 100};
    
    std::vector<MainOption> mMainOptions;
    int mCurrentClickedIndex = -1;
};

class AppManager{
    public:

    AppManager(int nWidth, int nHeight, Uint32 nFlags = 0, const char* pWindowsName = "App window");
    
    void AddImage(const std::string &imagePath);
    void NewCanvas(int width, int height);
    Canvas *GetCanvas();

    Uint32 GetWindowID(); 

    //When this function gets called, it is assumed that the window is focused. The canvas is passed in case it's needed
    void HandleEvent(SDL_Event *event);

    void Update(float deltaTime);

    void Draw();

    static void GetWindowSize(int &width, int &height);
    static int GetMinimumWindowY();
    static std::shared_ptr<TTF_Font> GetAppFont();
    
    //If the option couldn't be found, a null pointer is returned.
    static Option *FindOption(OptionInfo::OptionIDs optionID);

    //If the option couldn't be found, a null pointer is returned. pWindow gets set to the window holding the option, or nullptr if the option wasn't found
    static Option *FindOption(OptionInfo::OptionIDs optionID, InternalWindow *&pWindow);

    //Opens a text file with the name of the window from the InternalData forlder and creates a new window using that information 
    void InitializeWindow(const std::string &windowName);

    private:

    static int mWidth;
    static int mHeight;

    static int mMaximumWidth;
    static int mMaximumHeight;

    static std::shared_ptr<TTF_Font> mpFont;

    std::unique_ptr<SDL_Window, PointerDeleter> mpWindow;
    std::unique_ptr<SDL_Renderer, PointerDeleter> mpRenderer;

    //mpCanvas is a pointer to avoid dealing with default initialization
    std::unique_ptr<Canvas> mpCanvas;

    //May be changed into a SDL_Rect of the usable space (space unocupied by the main bar and anything else needed) in the future
    static int mMainBarHeight;
    std::unique_ptr<MainBar> mpMainBar;
    static std::vector<std::unique_ptr<InternalWindow>> mInternalWindows;

    bool HandleHotkeys(SDL_Event *pEvent);

    void InitializeFromFile();
    void ProcessMainBarData();
    void ProcessWindowsData();
    void ProcessCommandData(const std::string &commands);
};
/*
class AppDataRequester{
    public:

    static void SetApp(AppManager *npMainApp);
    static SDL_Color GetCanvasDrawingColor();
    
    static int GetCanvasWidth();
    static int GetCanvasHeight();

    static Pencil::PencilType GetPencilType();
    static int GetPencilRadius();
    static float GetPencilHardness();
    static Pencil::AlphaCalculation GetPencilAlphaCalculation();

    private:
    
    //We can use a pointer, since AppDataRequester is only thought to be used by elements inside AppManager, therefore the pointer should always be valid 
    static AppManager *mpMainApp;
};
*/
void MainLoop(AppManager &toolsWindow, std::span<char*> args);