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
#include <variant>
#include <unordered_map>
#include <functional>

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

//A button that can be clicked. It has no internal value that changes, other than its dimensions and its color
class ActionButton{
    public:

    SDL_Rect dimensions;

    ActionButton(SDL_Rect nDimensions);

    bool HandleEvent(SDL_Event *event);

    void Draw(SDL_Renderer *pRenderer);

    private:

    static constexpr SDL_Color IDLE_COLOR = {210, 210, 210};
    static constexpr SDL_Color HOLDED_COLOR = {160, 160, 160};
    SDL_Color mDrawColor = IDLE_COLOR;
};

//A button that, when clicked, its internal value switches from true to false and vice versa
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

struct OptionInfo{
    enum class OptionIDs : int{
        DRAWING_COLOR = 0,
        HARD_OR_SOFT = 1,
        TOOL_RADIUS = 2,
        PENCIL_HARDNESS = 3,
        SOFT_ALPHA_CALCULATION = 4,
        
        CHOOSE_TOOL = 20,

        ADD_LAYER = 50,
        REMOVE_CURRENT_LAYER = 51,
        SELECT_LAYER = 52,
        
        NEW_CANVAS_WIDTH = 100,
        NEW_CANVAS_HEIGHT = 101,
        NEW_CANVAS_CREATE = 102,

        SAVING_NAME = 200,
        PENCIL_DISPLAY_MAIN_COLOR = 201,
        PENCIL_DISPLAY_ALTERNATE_COLOR = 202,
        CANVAS_MOVEMENT_SPEED = 203,
        CANVAS_MOVEMENT_FAST_SPEED = 204
    };
    enum class DataUsed{
        NONE = 0,
        TEXT = 1,
        COLOR = 2,
        REAL_VALUE = 3,
        WHOLE_VALUE = 4,
        TICK = 5
    };
    using data_t = std::variant<std::monostate, std::string, SDL_Color, float, int, bool>;

    OptionIDs optionID; //Determines how the data should be handled

    data_t data;
    
    struct GetDataVisitor {
        template <typename T>
        T operator()(const T& value) const {
            return value;
        }
    };

    OptionInfo() : optionID((OptionIDs)(-1)), data(std::monostate{}){}
    
    template <typename T> requires std::is_constructible_v<data_t, T>
    OptionInfo(OptionIDs nOptionID, T nData) : optionID(nOptionID),  data(nData){}

    template <typename T> requires std::is_constructible_v<data_t, T>
    std::optional<T> GetData() const{
        if(std::holds_alternative<T>(data)){
            return std::get<T>(data);
        } else {
            return std::nullopt;
        }
    }

    bool IsInvalid(){return data.index() == static_cast<size_t>(DataUsed::NONE);}

    static OptionInfo Invalid(){return OptionInfo();}

    void SetTo(const OptionInfo& other){
        optionID = other.optionID;
        data = other.data;
    }
};

class Option{
    public:
    SDL_Point clickedPoint = {0,0};
    enum class InputMethod{
        INVALID = -1,
        TEXT_FIELD, //A text field for plain text, like a name
        HEX_TEXT_FIELD, //A text field in hexadecimal holding a color
        WHOLE_TEXT_FIELD, //A text field is used as input for numbers
        SLIDER, //A slider that can hold a value in a given range
        CHOICES_ARRAY, //An array of buttons to choose a preference (eg: tools)
        TICK, //A button that either has the value true or false is used as input
        ACTION //A button that performs an action when clicked
    };

    Option(int nTextWidth, SDL_Rect nDimensions, std::string_view nInfo = "");
    ~Option();

    bool HandleEvent(SDL_Event *event);
    void Draw(SDL_Renderer *pRenderer);

    void SetWidth(int nWidth);
    void SetHeight(int nHeight);
    void SetY(int nY);
    void SetOptionText(const char *pNewText);

    OptionInfo::OptionIDs GetOptionID();

    //Returns an InputMethod based on the character passed
    InputMethod CharToInputMethod(char inputIdentifier);

    void FetchInfo(std::string_view info);

    static void SetOptionsFont(std::shared_ptr<TTF_Font> npOptionsFont);

    private:

    static std::shared_ptr<TTF_Font> mpOptionsFont;

    //It is private because, if resize is needed, we would also need to resize other components
    SDL_Rect mDimensions;

    //Assigned in the constructor, it specifies which option is. It is only used in GetData() as the first 4 characters of the resulting string.
    //The handling of the different IDs should be done outside the Option class
    OptionInfo::OptionIDs mOptionID;

    //Gets set to true when any relevant data gets changed and therefore should be applied (example, activating a button to change from pencil to eraser)
    bool mModified = false;

    //If set to false, functions like Draw or HandleEvent will immediately return, in the latter the value returned is 'false'
    bool mActive = true;

    //Used to determine which member from the 'Input' union to use
    InputMethod mInputMethod;

    //This text displays next to the input to name the option
    std::unique_ptr<ConstantText> mOptionText; 

    //The width dedicated to the display of text
    const int M_TEXT_WIDTH;

    union Input{
        TextField *mpTextField = nullptr;
        Slider *mpSlider;
        ChoicesArray *mpChoicesArray;
        TickButton *mpTickButton;
        ActionButton *mpActionButton;
    } input;

    //TODO probably change, I don't think we just want to use hard coded values
    static constexpr int MIN_SPACE = 3;

    OptionInfo *GetData();

    void HandleInfo(std::string_view info);
    void SetInputMethod(InputMethod nInputMethod);
    
    //Class for the info handled
    class OptionCommands{
        public:

        static void LoadCommands();
        static void UnloadCommands();
        
        static void HandleCommand(Option *pOption, std::string_view command);

        static void SetActive(Option *pOption, std::string_view nActive);
        static void SetInitialValue(Option *pOption, std::string_view nValue);
        static void SetOptionText(Option *pOption, std::string_view nOptionText);
        static void SetMinValue(Option *pOption, std::string_view nMin);
        static void SetMaxValue(Option *pOption, std::string_view nMax);
        static void SetDecimalDigits(Option *pOption, std::string_view nDigits);
        static void SetDefaultText(Option *pOption, std::string_view nDefaultText);
        static void AddChoiceToArray(Option *pOption, std::string_view nTexturePath);
        static void UnusableInfo(Option *pOption, std::string_view nUnusableInfo);

        private:

        using t_commands_map = std::unordered_map<std::string_view, std::function<void(Option*, std::string_view)>>;
        static std::unique_ptr<t_commands_map> mCommandsMap;
    };
    
    //TODO: eventually find a work around, so that 'options.hpp' does not need a direct reference to a class present in 'engineInternals.hpp' (which includes 'options.hpp')
    friend class InternalWindow;

    //TODO: only a friend because of the Load/Unload Commands, eventually will be changed
    friend class AppManager;
};