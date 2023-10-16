#pragma once
#include "paintTools.hpp"
#include <array>
#include <span>
#include <functional>

//TODO: transform Data into a template
struct OptionInfo{
    enum class OptionIDs : int{
        DRAWING_COLOR = 0,
        HARD_OR_SOFT = 1,
        PENCIL_RADIUS = 2,
        PENCIL_HARDNESS = 3,
        SOFT_ALPHA_CALCULATION = 4,
        
        NEW_CANVAS_WIDTH = 100,
        NEW_CANVAS_HEIGHT = 101,
        NEW_CANVAS_CREATE = 102,
    };
    enum class DataUsed{
        TEXT,
        COLOR,
        REAL_VALUE,
        WHOLE_VALUE,
        TICK,
        NONE
    };

    OptionIDs optionID; //Determines how the data should be handled

    DataUsed dataUsed; //Determines which member from the Data union was assigned

    //Might be rewritten into just a std::string that is interpreted depending on "dataUsed"
    union Data{ //The data needed to be transmited
        std::string text; //The text of a textfield, like a layer's name
        SDL_Color color; //The color of a textfield or from the image itself
        float realValue; //The decimal value of a slider
        int wholeValue; //The whole value of a choices array or a text
        bool tick; //The value of a tickbutton

        Data(std::string nText) : text(nText){};
        Data(SDL_Color nColor) : color(nColor){};
        Data(float nRealValue) : realValue(nRealValue){};
        Data(int nWholeValue) : wholeValue(nWholeValue){};
        Data(bool nTick) : tick(nTick){};
        ~Data(){}
    } data;

    OptionInfo() : optionID((OptionIDs)(-1)), dataUsed(DataUsed::NONE), data(false){}
    
    template <typename T> requires std::is_constructible_v<Data, T>
    OptionInfo(OptionIDs nOptionID, DataUsed nDataUsed, T nData) : optionID(nOptionID), dataUsed(nDataUsed), data(nData){}
    
    ~OptionInfo(){
        if(dataUsed == DataUsed::TEXT){
            data.text.~basic_string();
        }
    }

    bool IsInvalid(){return dataUsed == DataUsed::NONE;}
    static OptionInfo Invalid(){return {(OptionIDs)(-1), DataUsed::NONE, false};}

    void SetTo(const OptionInfo& other){
        optionID = other.optionID;
        dataUsed = other.dataUsed;

        switch(dataUsed){
            case DataUsed::TEXT: data.text = other.data.text; break;
            case DataUsed::COLOR: data.color = other.data.color; break;
            case DataUsed::REAL_VALUE: data.realValue = other.data.realValue; break;
            case DataUsed::WHOLE_VALUE: data.wholeValue = other.data.wholeValue; break;
            case DataUsed::TICK: data.tick = other.data.tick; break;
        }
    }
};

//Possible bug that may arise: An option that handles an event before another option and is lower in the window, might register a mouse click as its own even if it should not
class Option{
    public:
    SDL_Point clickedPoint = {0,0};
    enum class InputMethod{
        HEX_TEXT_FIELD, //A text field in hexadecimal holding a color
        WHOLE_TEXT_FIELD, //A text field is used as input for numbers
        SLIDER, //A slider that can hold a value in a given range
        CHOICES_ARRAY, //An array of buttons to choose a preference (eg: tools)
        TICK //A button that either has the value true or false is used as input
    };

    Option(SDL_Rect nDimensions, std::string_view nInfo = "");
    ~Option();

    bool HandleEvent(SDL_Event *event);
    void Draw(SDL_Renderer *pRenderer);

    void SetWidth(int nWidth);
    void SetHeight(int nHeight);
    void SetOptionText(const char *pNewText);

    private:

    //It is private because, if resize is needed, we would also need to resize other components
    SDL_Rect mDimensions;

    //Assigned in the constructor, it specifies which option is. It is only used in GetData() as the first 4 characters of the resulting string.
    //The handling of the different IDs should be done outside the Option class
    OptionInfo::OptionIDs mOptionID;

    //Gets set to true when any relevant data gets changed and therefore should be applied (example, activating a button to change from pencil to eraser)
    bool mModified = false;

    //Used to determine which member from the 'Input' union to use
    InputMethod mInputMethod;

    //This text displays next to the input to name the option
    std::unique_ptr<ConstantText> mOptionText; 

    union Input{
        TextField *mpTextField = nullptr;
        Slider *mpSlider;
        ChoicesArray *mpChoicesArray;
        TickButton *mpTickButton;
    } input;

    //TODO probably change, I don't think we just want to use hard coded values
    static constexpr int MIN_SPACE = 3;
    static constexpr int OPTION_TEXT_WIDTH = 80;

    OptionInfo *GetData();

    void HandleInfo(std::string_view info);
    void SetInputMethod(InputMethod nInputMethod);
    
    //Class for the info handled
    class OptionCommands{
        public:

        static void LoadCommands();
        static void UnloadCommands();
        
        static void HandleCommand(Option *pOption, std::string_view command);

        static void SetOptionText(Option *pOption, std::string_view nOptionText);
        static void SetMinValue(Option *pOption, std::string_view nMin);
        static void SetMaxValue(Option *pOption, std::string_view nMax);
        static void SetDefaultText(Option *pOption, std::string_view nDefaultText);
        static void AddChoiceToArray(Option *pOption, std::string_view nTexturePath);
        static void UnusableInfo(Option *pOption, std::string_view nUnusableInfo);

        private:

        using t_commands_map = std::unordered_map<std::string_view, std::function<void(Option*, std::string_view)>>;
        static std::unique_ptr<t_commands_map> mCommandsMap;
    };

    friend class InternalWindow;
};

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

    InternalWindow(SDL_Point optionsSize, InitializationData nData);

    //Sets the InternalWindow's position to the chosen x and y, only if this does not crop the InternalWindow
    void SetPosition(int x, int y);
    void SetSize(int w, int h);

    void SetDimensions(const SDL_Rect &nDimensions);

    void AddTemporalData(OptionInfo *newData);
    std::vector<std::shared_ptr<OptionInfo>> &GetTemporalData();

    const std::string_view GetName();

    bool HandleEvent(SDL_Event *event);
    void Update(float deltaTime);
    void Draw(SDL_Renderer *pRenderer);

    std::vector<std::shared_ptr<OptionInfo>> GetData();

    //Converts the event x and y values to a value relative to the dimensions passed, saving the original position to a point parameter
    //Returns true if the event x or y values are outside the dimensions rect. In that case, they don't get changed
    static bool MakeEventRelativeToRect(const SDL_Rect &dimensions, int &eventX, int &eventY, SDL_Point &originalMousePosition, bool resetIfUnable = true);
    
    //Works exactly as the previous method, but it sets pEventX and pEventY to the location of eventX and eventY (so that the process can be undone)
    static bool MakeEventRelativeToRect(const SDL_Rect &dimensions, int &eventX, int &eventY, SDL_Point &originalMousePosition, int *&pEventX, int *&pEventY, bool resetIfUnable = true);

    private:

    SDL_Rect mDimensions = {0, 0, 200, 200};
    int mInnerBorder = 4;
    int mMinGap = mInnerBorder*2+1;
    SDL_Rect mContentDimensions = {0, 0, 100, 100};

    //Identifies this window
    const std::string M_WINDOW_NAME;

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

    void ProcessWindowInfo(std::string_view info);

    void UpdateContentDimensions();

    //Returns true if the given point falls exactly inside the inner border of the window
    //If border != nullptr, it gets assigned to the value of the exact border with the preferences being TOP > LEFT > BOTTOM > RIGHT
    bool PointInsideInnerBorder(SDL_Point point, Border *border);
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
        CLEAR = 0,
        NEW_CANVAS = 1
    };

    MainBar(SDL_Rect nDimensions);

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
    
    void AddImage(const char* pImage);
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

    //Opens a text file with the name of the window from the InternalData forlder and creates a new window using that information 
    void InitializeWindow(const std::string &windowName);

    private:

    static int mWidth;
    static int mHeight;

    static std::shared_ptr<TTF_Font> mpFont;

    std::unique_ptr<SDL_Window, PointerDeleter> mpWindow;
    std::unique_ptr<SDL_Renderer, PointerDeleter> mpRenderer;

    //mpCanvas is a pointer to avoid dealing with default initialization
    std::unique_ptr<Canvas> mpCanvas;

    //May be changed into a SDL_Rect of the usable space (space unocupied by the main bar and anything else needed) in the future
    static int mMainBarHeight;
    std::unique_ptr<MainBar> mpMainBar;
    static std::vector<std::unique_ptr<InternalWindow>> mInternalWindows;

    void ProcessMainBarData();
    void ProcessWindowsData();
};

void MainLoop(AppManager &toolsWindow, std::span<char*> args);