#pragma once
#include "paintTools.hpp"
#include <array>
#include <span>
#include <functional>

struct OptionInfo{
    enum class OptionIDs : unsigned int{
        DRAWING_COLOR = 0,
        HARD_OR_SOFT = 1,
        PENCIL_RADIUS = 2,
        PENCIL_HARDNESS = 3
    };
    enum class DataUsed{
        TEXT,
        COLOR,
        VALUE,
        TICK,
        NONE
    };

    const OptionIDs optionID; //Determines how the data should be handled

    const DataUsed dataUsed; //Determines which member from the Data union was assigned

    union Data{ //The data needed to be transmited
        const std::string text; //The text of a textfield, like a layer's name
        const SDL_Color color; //The color of a textfield or from the image itself
        const float value; //The value of a slider
        const bool tick; //The value of a tickbutton

        Data(std::string nText) : text(nText){};
        Data(SDL_Color nColor) : color(nColor){};
        Data(float nValue) : value(nValue){};
        Data(bool nTick) : tick(nTick){};
        ~Data(){}
    } data;

    OptionInfo() : optionID((OptionIDs)(-1)), dataUsed(DataUsed::NONE), data(false){}
    
    template <typename T> requires std::same_as<T, std::string> || std::same_as<T, SDL_Color> || std::same_as<T, float> || std::same_as<T, bool>
    OptionInfo(OptionIDs nOptionID, DataUsed nDataUsed, T nData) : optionID(nOptionID), dataUsed(nDataUsed), data(nData){}
    
    /*OptionInfo(OptionIDs nOptionID, DataUsed nDataUsed, std::string nData) : optionID(nOptionID), dataUsed(nDataUsed), data(nData){}
    OptionInfo(OptionIDs nOptionID, DataUsed nDataUsed, SDL_Color nData) : optionID(nOptionID), dataUsed(nDataUsed), data(nData){}
    OptionInfo(OptionIDs nOptionID, DataUsed nDataUsed, float nData) : optionID(nOptionID), dataUsed(nDataUsed), data(nData){}
    OptionInfo(OptionIDs nOptionID, DataUsed nDataUsed, bool nData) : optionID(nOptionID), dataUsed(nDataUsed), data(nData){}*/
    bool IsInvalid(){return dataUsed == DataUsed::NONE;}
    static OptionInfo Invalid(){return {(OptionIDs)(-1), DataUsed::NONE, false};}
};

//Possible bug that may arise: An option that handles an event before another option and is lower in the window, might register a mouse click as its own even if it should not
class Option{
    public:
    SDL_Point clickedPoint = {0,0};
    enum class InputMethod{
        HEX_TEXT_FIELD, //A text field in hexadecimal holding a color
        WHOLE_TEXT_FIELD, //A text field is used as input for numbers
        SLIDER, //A slider that can hold a value in a given range
        TICK //A button that either has the value true or false is used as input
    };

    Option(InputMethod nInputMethod, SDL_Rect nDimensions, OptionInfo::OptionIDs nOptionID, std::string nInfo = "");
    ~Option();

    bool HandleEvent(SDL_Event *event);
    void Draw(SDL_Renderer *pRenderer);

    void SetWidth(int nWidth);
    void SetHeight(int nHeight);
    void SetOptionText(const char *pNewText);

    std::shared_ptr<OptionInfo> GetData();

    private:

    //It is private because, if resize is needed, we would also need to resize other components
    SDL_Rect mDimensions;

    //Assigned in the constructor, it specifies which option is. It is only used in GetData() as the first 4 characters of the resulting string.
    //The handling of the different IDs should be done outside the Option class
    const OptionInfo::OptionIDs mOptionID;

    //Gets set to true when any relevant data gets changed and therefore should be applied (example, activating a button to change from pencil to eraser)
    bool mModified = false;

    //Used to determine which member from the 'Input' union to use
    const InputMethod mInputMethod;

    //This text displays next to the input to name the option
    std::unique_ptr<ConstantText> mOptionText; 

    union Input{
        TextField *mpTextField = nullptr;
        Slider *mpSlider;
        TickButton *mpTickButton;
    } input;

    //TODO probably change, I don't think we just want to use hard coded values
    static constexpr int MIN_SPACE = 3;
    static constexpr int OPTION_TEXT_WIDTH = 80;

    void HandleInfo(std::string &info);
    
    //Class for the info handled
    class OptionCommands{
        public:

        static void LoadCommands();
        static void UnloadCommands();
        
        static void HandleCommand(Option *pOption, const std::string &command);

        static void SetOptionText(Option *pOption, const std::string &nOptionText);
        static void SetMinValue(Option *pOption, const std::string &nMin);
        static void SetMaxValue(Option *pOption, const std::string &nMax);
        static void SetDefaultText(Option *pOption, const std::string &nDefaultText);
        static void UnusableInfo(Option *pOption, const std::string &nUnusableInfo);

        private:

        using t_commands_map = std::unordered_map<std::string, std::function<void(Option*, const std::string&)>>;
        static std::unique_ptr<t_commands_map> mCommandsMap;
    };

    friend class InternalWindow;
};

class InternalWindow{
    public:

    InternalWindow(SDL_Renderer *pRenderer);

    //Sets the InternalWindow's position to the chosen x and y, only if this does not crop the InternalWindow
    void SetPosition(int x, int y);
    void SetSize(int w, int h);

    void SetDimensions(const SDL_Rect &nDimensions);

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
    int M_MIN_GAP = mInnerBorder*2+1;
    SDL_Rect mContentDimensions = {0, 0, 100, 100};

    std::vector<std::unique_ptr<Option>> mOptions;

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

    void UpdateContentDimensions();

    //Returns true if the given point falls exactly inside the inner border of the window
    //If border != nullptr, it gets assigned to the value of the exact border with the preferences being TOP > LEFT > BOTTOM > RIGHT
    bool PointInsideInnerBorder(SDL_Point point, Border *border);
};

class AppManager{
    public:

    AppManager(int nWidth, int nHeight, Uint32 nFlags = 0, const char* pWindowsName = "App window");
    
    void AddImage(const char* pImage);
    Canvas *GetCanvas();

    Uint32 GetWindowID(); 

    //When this function gets called, it is assumed that the window is focused. The canvas is passed in case it's needed
    void HandleEvent(SDL_Event *event);

    void Update(float deltaTime);

    void Draw();

    static void GetWindowSize(int &width, int &height);
    static std::shared_ptr<TTF_Font> GetAppFont();

    private:

    static int mWidth;
    static int mHeight; 

    static std::shared_ptr<TTF_Font> mpFont;

    std::unique_ptr<SDL_Window, PointerDeleter> mpWindow;
    std::unique_ptr<SDL_Renderer, PointerDeleter> mpRenderer;

    //mpCanvas is a pointer to avoid dealing with default initialization
    std::unique_ptr<Canvas> mpCanvas;

    std::unique_ptr<TextField> mpColorField;
    std::unique_ptr<TextField> mpSizeField;
    std::unique_ptr<PositionPickerButton> mpColorPicker;

    std::array<std::unique_ptr<InternalWindow>, 1> mpWindows;

    void ProcessWindowsData();
};

void MainLoop(AppManager &toolsWindow, std::span<char*> args);