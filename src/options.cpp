#include "options.hpp"
#include "renderLib.hpp"
#include "logger.hpp"
#include <iomanip>
#include <functional>
#include <algorithm>
#include <charconv>

void *TextInputManager::mRequester = nullptr;

//CONSTANT TEXT METHODS:

ConstantText::ConstantText(const char *pText, std::shared_ptr<TTF_Font> pFont){
	Reset(pText, pFont);
}

void ConstantText::Reset(const char *pText, std::shared_ptr<TTF_Font> pFont){
	mpFont = pFont;

	TTF_SizeText(pFont.get(), pText, &mTextSize.x, &mTextSize.y);
	
	mpActualText = pText;
	mUpdateText = true;
}

void ConstantText::SetX(int x){
	mDimensions.x = x;
}
void ConstantText::SetY(int y){
	mDimensions.y = y;
}
void ConstantText::SetWidth(int nWidth){
	mDimensions.w = nWidth;
	mDimensions.h = mTextSize.y*nWidth/mTextSize.x;
}
void ConstantText::SetHeight(int nHeight){
	mDimensions.h = nHeight;
	mDimensions.w = mTextSize.x*nHeight/mTextSize.y;
}
int ConstantText::GetWidth(){
	return mDimensions.w;
}
SDL_Rect ConstantText::GetDimensions(){
	return mDimensions;
}

void ConstantText::Draw(SDL_Renderer *pRenderer){
	if(mUpdateText){
		mpTextTexture.reset(LoadTextureFromText(mpActualText.c_str(), pRenderer, mpFont.get()));
	}
	SDL_RenderCopy(pRenderer, mpTextTexture.get(), nullptr, &mDimensions);
}

//TEXT FIELD METHODS:

TextField::TextField(std::shared_ptr<TTF_Font> npFont, TextFormat nTextFormat, const std::string &nBlankText) : mpFont(npFont), mTextFormat(nTextFormat), mBlankText(nBlankText){}
TextField::~TextField(){
	TextInputManager::UnsetRequester(this);
}

void TextField::SetText(std::string_view nTextString){
	std::string sanitizedString;
	switch(mTextFormat){
		case TextFormat::HEX:
			//Sets 'mTextString' with all the digits from 'nTextString' that have a hexadecimal value and are part of the first 6 characters
			sanitizedString = nTextString.substr(0, std::min(nTextString.find_first_not_of("0123456789abcdefABCDEF"), (size_t)6));

			bool isValidData;
			SDL_Color nTextColor = GetAsColor(sanitizedString, &isValidData);
			if(isValidData) SetColor(nTextColor);

			break;
		case TextFormat::WHOLE_POSITIVE:
			sanitizedString = nTextString.substr(0, nTextString.find_first_not_of("0123456789"));
			break;

		case TextFormat::NONE:
			sanitizedString = nTextString;
			break;
	}

	if(sanitizedString != mTextString){
		mUpdateText = true;
		mTextString = sanitizedString;
	}

	mCursor.SetPotition(std::clamp(mCursor.GetPosition(), 0, (int)mTextString.size()));
}

void TextField::AppendText(std::string_view toAppendText){
	std::string resultingText = mTextString;
	resultingText.insert(mCursor.GetPosition(), toAppendText);
	
	mCursor.IncreasePosition(0, INT_MAX, toAppendText.size());
	SetText(resultingText);
}

void TextField::RemoveCharacters(int charactersAmount){
	std::string resultingText = mTextString;
	if(charactersAmount > mCursor.GetPosition()) charactersAmount = mCursor.GetPosition();
	resultingText.erase(resultingText.begin()+(mCursor.GetPosition()-charactersAmount), resultingText.begin()+mCursor.GetPosition());

	mCursor.DecreasePosition(0, mTextString.size(), charactersAmount);
	SetText(resultingText);
}

void TextField::SetBlankText(std::string_view nBlankText){
	mBlankText = nBlankText;

	if(mTextString.empty()) mUpdateText = true;
}

void TextField::SetTextFormat(TextFormat nTextFormat){
	mTextFormat = nTextFormat;
}

void TextField::SetColor(SDL_Color nTextColor){
	mTextColor = nTextColor;
}

std::string_view TextField::GetText(){
	return mTextString;
}

bool TextField::HasChanged(){
	return mUpdateText;
}

bool TextField::HandleEvent(SDL_Event *event){

	if(event->type == SDL_MOUSEBUTTONDOWN){
		SDL_Point mousePos = {event->button.x, event->button.y};

		if(SDL_PointInRect(&mousePos, &dimensions)){
			mSelected = true;
			TextInputManager::SetRequester(this);
		} else {
			mSelected = false;
			TextInputManager::UnsetRequester(this);
		}
		return mSelected;

	} else if(mSelected){
		if(!TextInputManager::IsRequester(this)){
			mSelected = false;
			return false;
		}

		if (event->type == SDL_TEXTINPUT){

			AppendText(event->text.text);

        	return true;

		} else if (event->type == SDL_KEYDOWN){
			bool isControlPressed = SDL_GetModState() & KMOD_CTRL;
			switch(event->key.keysym.sym){
				case SDLK_LEFT: 
					if(isControlPressed){
						size_t index = mTextString.find_last_of(' ', mCursor.GetPosition()-2);
						if(mCursor.GetPosition() < 2) index = -1;
						mCursor.DecreasePosition(0, mTextString.size(), mCursor.GetPosition() - 1 - index);
					} else {
						mCursor.DecreasePosition(0, mTextString.size());
					}
					break;
				case SDLK_RIGHT:
					if(isControlPressed){
						size_t index = mTextString.find_first_of(' ', mCursor.GetPosition()+1);
						mCursor.IncreasePosition(0, mTextString.size(), ((index == std::string::npos) ? mTextString.size() : index) - mCursor.GetPosition());
					} else {
						mCursor.IncreasePosition(0, mTextString.size());
					}
					break;
				case SDLK_BACKSPACE:
					if(!mTextString.empty()){
						if(isControlPressed){
						size_t index = mTextString.find_last_of(' ', mCursor.GetPosition()-2);
						if(mCursor.GetPosition() < 2) index = -1;
							RemoveCharacters(mCursor.GetPosition() - 1 - index);
						} else {
							RemoveCharacters(1);
						}
					}
					break;
				case SDLK_v: 
					if(isControlPressed){
						char *pClipboardText = SDL_GetClipboardText();
						AppendText(pClipboardText);
						SDL_free(pClipboardText);
					}
					break;
			}
		}
	}

	return false;
}

void TextField::Update(float deltaTime){

}

void TextField::Draw(SDL_Renderer *pRenderer){
	SDL_Rect cursorDimensions = {-1, -1, -1, -1};
	if(mUpdateText){
		if(!mTextString.empty()) mpTextTexture.reset(LoadTextureFromText(mTextString.c_str(), pRenderer, mpFont.get(), mTextColor));
		//If the text is empty, it renders the blank text
		else					 mpTextTexture.reset(LoadTextureFromText(mBlankText.c_str(), pRenderer, mpFont.get(), {150, 150, 150}));

		mUpdateText = false; 
	}
	
	//It doesn't make sense to keep rendering if the size is less or equal to 0
	if(dimensions.w <= 0 || dimensions.h <= 0){
		return;
	}

	int textW, textH; 

	if(!mTextString.empty()) {
		TTF_SizeUTF8(mpFont.get(), mTextString.c_str(), &textW, &textH);
	} else {
		TTF_SizeUTF8(mpFont.get(), mBlankText.c_str(), &textW, &textH);
	}

	if(displayBackground){
		SDL_Color background = {215, 215, 215};
		if(mTextColor.r + mTextColor.g + mTextColor.b > 382) background = {50, 50, 50};
		
		SDL_Rect displayRect = {(int)dimensions.x, (int)dimensions.y, (int)dimensions.w, (int)dimensions.h};
		SDL_SetRenderDrawColor(pRenderer, background.r, background.g, background.b, SDL_ALPHA_OPAQUE);
		SDL_RenderFillRect(pRenderer, &displayRect);

		SDL_SetRenderDrawColor(pRenderer, background.r-50, background.g-50, background.b-50, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(pRenderer, &displayRect);
	}

	const float TEXT_X_PADDING = dimensions.h/6.25f;
	SDL_Rect textRect = {(int)TEXT_X_PADDING, 0, (textW*dimensions.h)/textH, dimensions.h};
	
	SDL_Rect previousViewport;
	SDL_RenderGetViewport(pRenderer, &previousViewport);

	SDL_Rect realDimensions = {(int)dimensions.x + previousViewport.x, (int)dimensions.y + previousViewport.y, (int)dimensions.w, (int)dimensions.h};

	//First we check if the text is supposed to render (aka is inside the viewport)
	if(SDL_Rect resultingDimensions; SDL_IntersectRect(&previousViewport, &realDimensions, &resultingDimensions) == SDL_TRUE){
		SDL_RenderSetViewport(pRenderer, &resultingDimensions);
		SDL_RenderCopy(pRenderer, mpTextTexture.get(), nullptr, &textRect);

		//Easy way of removing the cursor on sliders, may be changed into the future (maybe by changing what sliders use to display value in text, maybe by letting them be interactable, maybe doesnby having its own bool)
		if(displayBackground && mSelected){
			mCursor.Draw(pRenderer, mTextColor, dimensions.h, mpFont.get(), mTextString);
		}

		SDL_RenderSetViewport(pRenderer, &previousViewport);
	}
}

bool TextField::IsValidNumber(){
	if(mTextString.empty()) return false;
	switch (mTextFormat)
	{
		case TextFormat::HEX:
			for(auto ch : mTextString){
				if(!std::isxdigit(ch)){
					return false;
				}
			}
			break;	
		case TextFormat::WHOLE_POSITIVE:
			for(auto ch : mTextString){
				if(!std::isdigit(ch)){
					return false;
				}
			}
			break;	
		default:
			ErrorPrint("text format uniable to generate number");
			return false;
	}
	return true;
}

int TextField::GetAsNumber(bool *isValidData){
	int rtNumber = 0;
	
	//No point on calling IsValidNumber() as it would require going trough the whole text twice
	try{
		switch (mTextFormat)
		{
			case TextFormat::HEX:
				rtNumber = stoi(mTextString, nullptr, 16);
				break;	
			case TextFormat::WHOLE_POSITIVE:
				rtNumber = stoi(mTextString);
				break;	
			default:
				ErrorPrint("text format uniable to generate number");
				if(isValidData) *isValidData = false;
				return -1;
		}
		//at this point we can be sure it has been converted as no exceptions have been thrown
		if(isValidData) *isValidData = true;
	} catch(const std::invalid_argument &e){
		//means that 'mTextString' cannot be converted into an int
		DebugPrint("Cannot convert \"" + mTextString + "\" into an int");
		if(isValidData) *isValidData = false;
	} catch(const std::out_of_range &e){
		//means that 'mTextString' is to big of a number to transform into an int
		DebugPrint('\"' + mTextString + "\" overflows when turned into an int");
		if(isValidData) *isValidData = false;
	}

	return rtNumber;
}

bool TextField::IsValidColor(){
	return IsValidColor(mTextString);
}

SDL_Color TextField::GetAsColor(bool *isValidData){
	return GetAsColor(mTextString, isValidData);
}

bool TextField::IsValidColor(std::string text){
	if(text.size() != 6){
		return false;
	}

	//Should always be true, as the format cannot be changed and is set to hex
	for(const auto& digit : text){
		if(!std::isxdigit(digit)){
			return false;
		}
	}

	return true;
}

SDL_Color TextField::GetAsColor(std::string text, bool *isValidData){
	SDL_Color rtColor {0, 0, 0, SDL_ALPHA_OPAQUE};
	bool isHex = IsValidColor(text);
	
	if(isHex){
		rtColor.r = std::stoul(text.substr(0, 2), nullptr, 16);
		rtColor.g = std::stoul(text.substr(2, 2), nullptr, 16);
		rtColor.b = std::stoul(text.substr(4, 2), nullptr, 16);
	}

	//We check if 'isValidData' is not a nullptr, and set it to the value of 'isHex'
	if(isValidData) *isValidData = isHex;

	return rtColor;
}

//TEXTFIELD CURSOR METHODS

TextField::Cursor::Cursor(int nPosition) : mPosition(nPosition){}
            
void TextField::Cursor::SetPotition(int nPosition){
	mPosition = nPosition;
}

int TextField::Cursor::GetPosition(){
	return mPosition;
}

void TextField::Cursor::DecreasePosition(int minPosition, int maxPosition, int amount){
	mPosition -= amount;
	if(mPosition < minPosition || mPosition > maxPosition){
		mPosition = minPosition;
	}
}

void TextField::Cursor::IncreasePosition(int minPosition, int maxPosition, int amount){
	mPosition += amount;
	if(mPosition < minPosition || mPosition > maxPosition){
		mPosition = maxPosition;
	}
}

void TextField::Cursor::Draw(SDL_Renderer* pRenderer, SDL_Color cursorColor, int height, TTF_Font *pFont, const std::string& text){
	//May change into the future, making it a parametter or a constexpr function
	const int TEXT_X_PADDING = height/6.25f;

	SDL_Rect cursorDimensions = {TEXT_X_PADDING, 0, 2, height};

	if(!text.empty()) {
		int textW, textH;
		TTF_SizeUTF8(pFont, text.substr(0, mPosition).c_str(), &textW, &textH);
		cursorDimensions.x += (textW*height)/textH;
	}

	//We don't use the alpha value because it aready has no effect on the text
	SDL_SetRenderDrawColor(pRenderer, cursorColor.r, cursorColor.g, cursorColor.b, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRect(pRenderer, &cursorDimensions);
}

//ACTION BUTTON METHODS:

ActionButton::ActionButton(SDL_Rect nDimensions) : dimensions(nDimensions){}

bool ActionButton::HandleEvent(SDL_Event *event){
	if(event->type == SDL_MOUSEBUTTONDOWN){
		SDL_Point mousePos = {event->button.x, event->button.y};

		if(SDL_PointInRect(&mousePos, &dimensions)){
			mDrawColor = HOLDED_COLOR;
			return true;
		}
	} else if (event->type == SDL_MOUSEBUTTONUP){
		mDrawColor = IDLE_COLOR;
	}
	return false;
}

void ActionButton::Draw(SDL_Renderer *pRenderer){
	SDL_SetRenderDrawColor(pRenderer, mDrawColor.r, mDrawColor.g, mDrawColor.b, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRect(pRenderer, &dimensions);
	SDL_SetRenderDrawColor(pRenderer, 120, 120, 120, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(pRenderer, &dimensions);
}

//TICK BUTTON METHODS:

TickButton::TickButton(SDL_Rect nDimensions, bool nValue) : dimensions(nDimensions), mValue(nValue){}

bool TickButton::HandleEvent(SDL_Event *event){
	if(event->type == SDL_MOUSEBUTTONDOWN){
		SDL_Point mousePos = {event->button.x, event->button.y};

		if(SDL_PointInRect(&mousePos, &dimensions)){
			mValue = !mValue;
			return true;
		}
	}
	return false;
}

void TickButton::Draw(SDL_Renderer *pRenderer){
	Uint8 innerBright = 255*(mValue?1:0);

	SDL_SetRenderDrawColor(pRenderer, innerBright, innerBright, innerBright, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRect(pRenderer, &dimensions);
	SDL_SetRenderDrawColor(pRenderer, 127, 127, 127, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(pRenderer, &dimensions);
}

void TickButton::SetValue(bool nValue){
	mValue = nValue;
}

bool TickButton::GetValue(){
	return mValue;
}

//SLIDER METHODS:

Slider::Slider(std::shared_ptr<TTF_Font> npFont, SDL_Rect nDimensions, float initialValue, float nMin, float nMax) :
	mTextField(npFont, TextField::TextFormat::NONE, "ERROR"), mDimensions(nDimensions), mFilledDimensions(nDimensions), mMin(nMin), mMax(nMax){
	
	mTextField.dimensions = mDimensions;
	mTextField.displayBackground = false;
	mTextField.SetColor({255, 255, 255});
	
	SetValue(initialValue);
}

void Slider::SetWidth(int nWidth){
	mDimensions.w = nWidth;
	if(mMax != mMin) mFilledDimensions.w = (int)(mDimensions.w * (mValue-mMin)/(mMax-mMin));
	else mFilledDimensions.w = mDimensions.w;
	mTextField.dimensions.w = mDimensions.w;
}

void Slider::SetDimensions(SDL_Rect nDimensions){
	mDimensions = nDimensions;
	mFilledDimensions = nDimensions;
	if(mMax != mMin) mFilledDimensions.w = (int)(mDimensions.w * (mValue-mMin)/(mMax-mMin));
	else mFilledDimensions.w = mDimensions.w;
	mTextField.dimensions = mDimensions;
}

SDL_Rect Slider::GetDimensions(){
	return mDimensions;
}

void Slider::SetValue(float nValue, bool mustUpdate){
	//We adjusti it so that it only conserves the most important digits
	nValue = std::round(std::pow(10, mDecimalPlaces)*std::clamp(nValue, mMin, mMax))*std::pow(0.1f, mDecimalPlaces);

	//Then we check if the value has changed in any way
	mHasChanged = (mValue != nValue);
	if(!mHasChanged && !mustUpdate){
		return;
	}
	mValue = nValue;

	//We set the filled dimensions width
	if(mMax != mMin) mFilledDimensions.w = (int)(mDimensions.w * (nValue-mMin)/(mMax-mMin));
	else mFilledDimensions.w = mDimensions.w;

	//We transform the value into a string and finally we get only the part we want (all digits except the decimal places that we do not want)
	//Even if before we set mValue's precision, the float still holds those decimal values (either as 0s or 9s), so we still have to truncate them
	std::string textValue;
	if(mDecimalPlaces > 0) {
		textValue = std::to_string(mValue);
		mTextField.SetText(textValue.substr(0, textValue.find('.')+(mDecimalPlaces+1)));
	}
	else{ 
		textValue = std::to_string((int)mValue);
		mTextField.SetText(textValue);
	}
}

void Slider::SetMinValue(float nMin){
	mMin = nMin;
	
	//We make sure the value is clamped + recalculate dimensions and text
	SetValue(mValue, true);
}
void Slider::SetMaxValue(float nMax){
	mMax = nMax;

	//We make sure the value is clamped + recalculate dimensions and text
	SetValue(mValue, true);
}

void Slider::SetDecimalPlaces(int nDecimalPlaces){
	mDecimalPlaces = nDecimalPlaces;
	
	//We make sure the value is adjuste + recalculate (dimensions and) text
	SetValue(mValue, true);
}

float Slider::GetValue(){
	return mValue;
}

bool Slider::HasChanged(){
	return mHasChanged;
}

bool Slider::HandleEvent(SDL_Event *event){
	if(event->type == SDL_MOUSEBUTTONDOWN){
		SDL_Point mousePos = {event->button.x, event->button.y};

		if(SDL_PointInRect(&mousePos, &mDimensions)){
			SetValue((mMax-mMin)*((mousePos.x-mDimensions.x)/(float)mDimensions.w)+mMin);
			mSelected = true;
		}
	} else if(event->type == SDL_MOUSEWHEEL){
		SetValue(mValue + event->wheel.y*std::pow(0.1f, mDecimalPlaces));
		return true;
	}
	else if(mSelected){
		if(event->type == SDL_MOUSEMOTION){
			if(event->motion.x < mDimensions.x){
				//We check for mValue being equal to mMin as it is faster than the method used in SetValue to verify it has changed
				if(mValue != mMin) SetValue(mMin);
			}
			else if (event->motion.x > mDimensions.x+mDimensions.w){
				//We check for mValue being equal to mMax as it is faster than the method used in SetValue to verify it has changed
				if(mValue != mMax) SetValue(mMax);
			}
			else {
				SetValue((mMax-mMin)*((event->motion.x-mDimensions.x)/(float)mDimensions.w)+mMin);
			}
		}
		else if(event->type == SDL_MOUSEBUTTONUP){
			mSelected = false;
		}
	}

	return mSelected;
}

void Slider::Draw(SDL_Renderer *pRenderer){
	FillRect(pRenderer, mDimensions, 20, 20, 20);
	FillRect(pRenderer, mFilledDimensions, 200, 100, 60);

	//Make a viewport with the dimensions, so that the textfield appears clipped instead of outside the slider
	SDL_Rect previousViewport = {-1, -1, -1, -1};
	SDL_RenderGetViewport(pRenderer, &previousViewport);
	SDL_Rect newViewport = {previousViewport.x, previousViewport.y, std::min(mDimensions.x+mDimensions.w, previousViewport.w), previousViewport.h};
	
	SDL_RenderSetViewport(pRenderer, &newViewport);

	mTextField.Draw(pRenderer);

	SDL_RenderSetViewport(pRenderer, &previousViewport);
}

//CHOICES_ARRAY METHODS:

ChoicesArray::ChoicesArray(SDL_Rect nDimensions, int nButtonsSize) : mDimensions(nDimensions), mButtonsSize(nButtonsSize){}

void ChoicesArray::AddOption(std::string texturePath){
	mTexturesPaths.push_back(texturePath);
	mUpdateTextures = true;
}

void ChoicesArray::SetDimensions(SDL_Rect nDimensions){
	mDimensions = nDimensions;
}

SDL_Rect ChoicesArray::GetDimensions(){
	return mDimensions;
}

bool ChoicesArray::SetLastChosenOption(int nLastChosen){
	if(nLastChosen >= mTextures.size()){
		return true;
	}

	//We check that mLastChosen was valid in order to set the texture back to its original color mod
	if(mLastChosen < mTextures.size()){
		//We set the previously chosen to its original color
		SDL_SetTextureColorMod(mTextures[mLastChosen].get(), 255, 255, 255);
	}
	
	//We make the currently selected a bit darker
	SDL_SetTextureColorMod(mTextures[nLastChosen].get(), 180, 180, 180);
	
	mLastChosen = nLastChosen;
	
	return false;
}

int ChoicesArray::GetLastChosenOption(){
	return mLastChosen;
}

void ChoicesArray::UncheckedSetLastChosenOption(int nLastChosen){
	mLastChosen = nLastChosen;
}

bool ChoicesArray::HandleEvent(SDL_Event *event){
	if(event->type == SDL_MOUSEBUTTONDOWN){
		SDL_Point mousePos = {event->button.x, event->button.y};

		SDL_Rect buttonSpace = {mDimensions.x, mDimensions.y , mButtonsSize*(int)floor(mDimensions.w/(float)mButtonsSize), mButtonsSize*(int)ceil(mDimensions.h/(float)mButtonsSize)};

		if(SDL_PointInRect(&mousePos, &buttonSpace)){

			//We calculate the index of the button in the clicked space
			int deltaX = (mousePos.x - buttonSpace.x) / mButtonsSize;
			int deltaY = (mousePos.y - buttonSpace.y) / mButtonsSize;
			int newChosen = deltaY * floor(mDimensions.w/mButtonsSize) + deltaX;
			
			//If there was no error with the new choice, aka a new choice was set, we return true
			return !SetLastChosenOption(newChosen);
		}
	}
	return false;
}

void ChoicesArray::Draw(SDL_Renderer *pRenderer){
	if(mUpdateTextures){
		UpdateTextures(pRenderer);
	}

	SDL_Rect buttonRect = {mDimensions.x, mDimensions.y, mButtonsSize, mButtonsSize};

	for(const auto &texture : mTextures){
		SDL_RenderCopy(pRenderer, texture.get(), nullptr, &buttonRect);
		
		buttonRect.x += mButtonsSize;
		if(buttonRect.x + mButtonsSize > mDimensions.x + mDimensions.w){
			buttonRect.x = mDimensions.x;
			buttonRect.y += mButtonsSize;

			if(buttonRect.y >= mDimensions.y + mDimensions.h){
				//As the button wouldn't display, we break out of the loop
				break;
			}
		}
	}
}

void ChoicesArray::UpdateTextures(SDL_Renderer *pRenderer){
	mTextures.resize(mTexturesPaths.size());
	for(int i = 0; i < mTexturesPaths.size(); ++i){
		mTextures[i].reset(LoadTexture(mTexturesPaths[i].c_str(), pRenderer));
	}

	mTexturesPaths.clear();
	mUpdateTextures = false;
	
	//We call this function to update the colouring of the selected texture that was just resseted
	SetLastChosenOption(mLastChosen);
}

std::shared_ptr<TTF_Font> Option::mpOptionsFont;
//OPTION METHODS:

Option::Option(int nTextWidth, SDL_Rect nDimensions, std::string_view nInfo) : M_TEXT_WIDTH(nTextWidth), mDimensions(nDimensions){
	HandleInfo(nInfo);
}

Option::~Option(){
	switch(mInputMethod){
		case InputMethod::TEXT_FIELD:
		case InputMethod::HEX_TEXT_FIELD:
		case InputMethod::WHOLE_TEXT_FIELD:
			delete input.mpTextField;
			break;

		case InputMethod::SLIDER:
			delete input.mpSlider;
			break;

		case InputMethod::CHOICES_ARRAY:
			delete input.mpChoicesArray;
			break;

		case InputMethod::TICK:
			delete input.mpTickButton;
			break;

		case InputMethod::ACTION:
			delete input.mpActionButton;
			break;
	}
}

bool Option::HandleEvent(SDL_Event *event){
	SDL_Point originalMousePosition;
	int *eventMouseX = nullptr, *eventMouseY = nullptr;

	bool eventHandled = false;
	bool wasOptionClicked = false;

	switch (event->type)
	{
		case SDL_MOUSEBUTTONDOWN:
			if (MakeEventRelativeToRect(mDimensions, event->button.x, event->button.y, originalMousePosition, eventMouseX, eventMouseY)) return false;
			else wasOptionClicked = true; //We don't need other options handling the button down if this one already did
			break;
		case SDL_MOUSEBUTTONUP:
			//We want to know when the mouse is up, even if it is outside the option
			MakeEventRelativeToRect(mDimensions, event->motion.x, event->motion.y, originalMousePosition, eventMouseX, eventMouseY, false);
			break;
		case SDL_MOUSEMOTION:
			//We want to keep track of the mouse motion, even if it is outside the option
			MakeEventRelativeToRect(mDimensions, event->motion.x, event->motion.y, originalMousePosition, eventMouseX, eventMouseY, false);
			break;
		case SDL_MOUSEWHEEL:
			if (MakeEventRelativeToRect(mDimensions, event->wheel.mouseX, event->wheel.mouseY, originalMousePosition, eventMouseX, eventMouseY)) return false;
			break;
	}

	switch(mInputMethod){
		case InputMethod::TEXT_FIELD:
			eventHandled = input.mpTextField->HandleEvent(event);
			if(input.mpTextField->HasChanged()) mModified = true;
			break;
		case InputMethod::HEX_TEXT_FIELD:
			eventHandled = input.mpTextField->HandleEvent(event);
			if(input.mpTextField->HasChanged() && input.mpTextField->IsValidColor()) mModified = true;
			break;

		case InputMethod::WHOLE_TEXT_FIELD:
			eventHandled = input.mpTextField->HandleEvent(event);
			if(input.mpTextField->HasChanged() && input.mpTextField->IsValidNumber()) mModified = true;
			break;

		case InputMethod::SLIDER:
			eventHandled = input.mpSlider->HandleEvent(event);
			if(eventHandled && input.mpSlider->HasChanged()) mModified = true;
			break;

		case InputMethod::CHOICES_ARRAY:
			eventHandled = input.mpChoicesArray->HandleEvent(event);
			if(eventHandled) mModified = true;
			break;

		case InputMethod::TICK:
			eventHandled = input.mpTickButton->HandleEvent(event);
			if(eventHandled) mModified = true;
			break;

		case InputMethod::ACTION:
			eventHandled = input.mpActionButton->HandleEvent(event);
			if(eventHandled) mModified = true;
			break;
	}

	//Finally we undo the process of making it relative
	if(eventMouseX != nullptr){
		*eventMouseX = originalMousePosition.x;
		*eventMouseY = originalMousePosition.x;
	}

	return eventHandled || wasOptionClicked;
}
void Option::Draw(SDL_Renderer *pRenderer){
	SDL_Rect previousViewport = {-1, -1, -1, -1};
	SDL_RenderGetViewport(pRenderer, &previousViewport); //A window's rect

	if(previousViewport.w == -1){
		ErrorPrint("'previousViewport' doesn't exist");
		return;
	}

	if(SDL_Rect adjustedDimensions = {previousViewport.x + mDimensions.x, previousViewport.y + mDimensions.y, mDimensions.w, mDimensions.h}, intersectionRect;
	   SDL_IntersectRect(&previousViewport, &adjustedDimensions, &intersectionRect) == SDL_TRUE){

		//Renders red the viewport of the option for debugging purposes
		//SDL_RenderSetViewport(pRenderer, nullptr);
		//SDL_SetRenderDrawColor(pRenderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
		//SDL_RenderDrawRect(pRenderer, &intersectionRect);

		SDL_RenderSetViewport(pRenderer, &intersectionRect);
	}
	else return; //We return as it means that the option is not even visible

	if(mOptionText) {
		SDL_Rect optionRect, textRect = mOptionText->GetDimensions();
		SDL_RenderGetViewport(pRenderer, &optionRect);
		
		if(SDL_Rect adjustedDimensions = {optionRect.x, optionRect.y, textRect.x + M_TEXT_WIDTH, textRect.y + textRect.h}, intersectionRect;
			SDL_IntersectRect(&previousViewport, &adjustedDimensions, &intersectionRect) == SDL_TRUE){
			
			SDL_RenderSetViewport(pRenderer, &intersectionRect);
			mOptionText->Draw(pRenderer);
			SDL_RenderSetViewport(pRenderer, &optionRect);
		}
	}

	switch(mInputMethod){
		case InputMethod::TEXT_FIELD:
		case InputMethod::HEX_TEXT_FIELD:
		case InputMethod::WHOLE_TEXT_FIELD:
			input.mpTextField->Draw(pRenderer);
			break;

		case InputMethod::SLIDER:
			input.mpSlider->Draw(pRenderer);
			break;

		case InputMethod::CHOICES_ARRAY:
			input.mpChoicesArray->Draw(pRenderer);
			break;

		case InputMethod::TICK:
			input.mpTickButton->Draw(pRenderer);
			break;

		case InputMethod::ACTION:
			input.mpActionButton->Draw(pRenderer);
			break;
	}

	SDL_RenderSetViewport(pRenderer, &previousViewport);
}

void Option::SetWidth(int nWidth){
	mDimensions.w = nWidth;
	nWidth -= MIN_SPACE;

	switch(mInputMethod){
		case InputMethod::TEXT_FIELD:
		case InputMethod::HEX_TEXT_FIELD:
		case InputMethod::WHOLE_TEXT_FIELD:
			input.mpTextField->dimensions.w = nWidth-input.mpTextField->dimensions.x;
			break;
			
		case InputMethod::SLIDER:
			input.mpSlider->SetWidth(std::max(nWidth-input.mpSlider->GetDimensions().x, MIN_SPACE));
			break;

		case InputMethod::CHOICES_ARRAY:{
			SDL_Rect choicesArrayDimensions = input.mpChoicesArray->GetDimensions();
			choicesArrayDimensions.w = nWidth;
			input.mpChoicesArray->SetDimensions(choicesArrayDimensions);
			break;
		}

		case InputMethod::TICK: //We don't do anything as (currently) the height of the option determines the dimensions of the tick button
		case InputMethod::ACTION: //We don't do anything as (currently) the height of the option determines the dimensions of the action button
			break;
	}
}

void Option::SetHeight(int nHeight){
	mDimensions.h = nHeight;
	nHeight -= MIN_SPACE;
	//TODO: (may) have a bool or some way to tell the owning window that the height has been changed so it can reorder the options height

	switch(mInputMethod){
		case InputMethod::TEXT_FIELD:
		case InputMethod::HEX_TEXT_FIELD:
		case InputMethod::WHOLE_TEXT_FIELD:
			input.mpTextField->dimensions.h = nHeight;
			break;
			
		case InputMethod::SLIDER:{
			SDL_Rect sliderDimensions = input.mpSlider->GetDimensions();
			sliderDimensions.h = nHeight;
			input.mpSlider->SetDimensions(sliderDimensions);
			break;
		}

		case InputMethod::CHOICES_ARRAY:{
			SDL_Rect choicesArrayDimensions = input.mpChoicesArray->GetDimensions();
			choicesArrayDimensions.h = nHeight;
			input.mpChoicesArray->SetDimensions(choicesArrayDimensions);
			break;
		}

		case InputMethod::TICK:
			input.mpTickButton->dimensions.w = mDimensions.h-MIN_SPACE*2;
			input.mpTickButton->dimensions.h = mDimensions.h-MIN_SPACE*2;
			break;

		case InputMethod::ACTION:
			input.mpActionButton->dimensions.w = (mDimensions.h-MIN_SPACE)*2;
			input.mpActionButton->dimensions.h = mDimensions.h-MIN_SPACE*2;
			break;
	}
}

void Option::SetOptionText(const char *pNewText){
	
	mOptionText.reset(new ConstantText(pNewText, mpOptionsFont));
	mOptionText->SetX(MIN_SPACE);
	mOptionText->SetY(MIN_SPACE);
	mOptionText->SetHeight(mDimensions.h-MIN_SPACE*2);
	
	int maxTextSpace = M_TEXT_WIDTH;

	switch(mInputMethod){
		case InputMethod::TEXT_FIELD:
		case InputMethod::HEX_TEXT_FIELD:
		case InputMethod::WHOLE_TEXT_FIELD:
			
			input.mpTextField->dimensions.x += MIN_SPACE + maxTextSpace;
			input.mpTextField->dimensions.w -= MIN_SPACE + maxTextSpace;
			break;
			
		case InputMethod::SLIDER:{
			
			SDL_Rect sliderDimensions = input.mpSlider->GetDimensions();
			sliderDimensions.x += MIN_SPACE + maxTextSpace;
			sliderDimensions.w -= MIN_SPACE + maxTextSpace;
			input.mpSlider->SetDimensions(sliderDimensions);
			
			break;
		}

		case InputMethod::CHOICES_ARRAY:
			SDL_Rect choicesArrayDimensions = input.mpChoicesArray->GetDimensions();
			choicesArrayDimensions.x += MIN_SPACE + maxTextSpace;
			choicesArrayDimensions.w -= MIN_SPACE + maxTextSpace;
			input.mpChoicesArray->SetDimensions(choicesArrayDimensions);
			break;

		case InputMethod::TICK:
			input.mpTickButton->dimensions.x += MIN_SPACE + maxTextSpace;
			break;

		case InputMethod::ACTION:
			input.mpActionButton->dimensions.x += MIN_SPACE + maxTextSpace;
			break;
	}
}

OptionInfo::OptionIDs Option::GetOptionID(){
	return mOptionID;
}

void Option::FetchInfo(std::string_view info){
	Option::OptionCommands::LoadCommands();

	for(int prevI = 0, i = info.find_first_of('_', prevI); i != std::string::npos; i = info.find_first_of('_', prevI)){
		Option::OptionCommands::HandleCommand(this, info.substr(prevI, i-prevI));
		
		prevI = i+1;
	}

	Option::OptionCommands::UnloadCommands();
}

void Option::SetOptionsFont(std::shared_ptr<TTF_Font> npOptionsFont){
    mpOptionsFont.reset(npOptionsFont.get(), PointerDeleter{});
}

OptionInfo *Option::GetData(){
	if(mModified) mModified = false;
	else return new OptionInfo();

	switch(mInputMethod){
		case InputMethod::TEXT_FIELD:
			return new OptionInfo(mOptionID, std::string(input.mpTextField->GetText()));

		case InputMethod::HEX_TEXT_FIELD:
			return new OptionInfo(mOptionID, input.mpTextField->GetAsColor(nullptr));

		case InputMethod::WHOLE_TEXT_FIELD:
			return new OptionInfo(mOptionID, input.mpTextField->GetAsNumber(nullptr));

		case InputMethod::SLIDER:
			return new OptionInfo(mOptionID, input.mpSlider->GetValue());

		case InputMethod::CHOICES_ARRAY:
			return new OptionInfo(mOptionID, input.mpChoicesArray->GetLastChosenOption());

		case InputMethod::TICK:
			return new OptionInfo(mOptionID, input.mpTickButton->GetValue());

		case InputMethod::ACTION:
			return new OptionInfo(mOptionID, true);

		default:
			return new OptionInfo();
	}
}

void Option::HandleInfo(std::string_view info){
	//If there is no information to handle, we just return
	if(info.empty()) return;

	int previousIndex = 0, currentIndex = info.find_first_of('_', previousIndex);
	
	//We read and set the option id
	int optionID;
	std::from_chars(info.data()+previousIndex, info.data()+currentIndex, optionID);
	mOptionID = static_cast<OptionInfo::OptionIDs>(optionID);

	previousIndex = currentIndex+1;
	currentIndex = info.find_first_of('_', previousIndex);
	char inputMethod = info[previousIndex];

	switch(inputMethod){
		case 'F':
			SetInputMethod(InputMethod::TEXT_FIELD); break;
		case 'H':
			SetInputMethod(InputMethod::HEX_TEXT_FIELD); break;
		case 'W':
			SetInputMethod(InputMethod::WHOLE_TEXT_FIELD); break;
		case 'T':
			SetInputMethod(InputMethod::TICK); break;
		case 'A':
			SetInputMethod(InputMethod::ACTION); break;
		case 'S':
			SetInputMethod(InputMethod::SLIDER); break;	
		case 'C':
			SetInputMethod(InputMethod::CHOICES_ARRAY); break;
		default:
			ErrorPrint("Invalid input method: "+inputMethod); break;
	}

	//We set the index ready for the next info and declare i, the variable that will be used inside the loop 
	previousIndex = currentIndex+1;
	
	//We loop over all the data that until there is no more
	for(int i = info.find_first_of('_', previousIndex); i != std::string::npos; i = info.find_first_of('_', previousIndex)){
		
		Option::OptionCommands::HandleCommand(this, info.substr(previousIndex, i-previousIndex));
		
		previousIndex = i+1;
	}
}

//TODO: create a function/method for each input method, so that calculating its dimensions is consistent across different methods
void Option::SetInputMethod(Option::InputMethod nInputMethod){
	mInputMethod = nInputMethod;
	switch(mInputMethod){
		case InputMethod::TEXT_FIELD:
			input.mpTextField = new TextField(mpOptionsFont, TextField::TextFormat::NONE);
			input.mpTextField->dimensions = {MIN_SPACE, MIN_SPACE, mDimensions.w-MIN_SPACE*2, mDimensions.h-MIN_SPACE*2};
			input.mpTextField->SetColor({0, 0, 0});
			break;
		case InputMethod::HEX_TEXT_FIELD:
			input.mpTextField = new TextField(mpOptionsFont, TextField::TextFormat::HEX);
			input.mpTextField->dimensions = {MIN_SPACE, MIN_SPACE, mDimensions.w-MIN_SPACE*2, mDimensions.h-MIN_SPACE*2};
			input.mpTextField->SetColor({0, 0, 0});
			break;

		case InputMethod::WHOLE_TEXT_FIELD:
			input.mpTextField = new TextField(mpOptionsFont, TextField::TextFormat::WHOLE_POSITIVE);
			input.mpTextField->dimensions = {MIN_SPACE, MIN_SPACE, mDimensions.w-MIN_SPACE*2, mDimensions.h-MIN_SPACE*2};
			input.mpTextField->SetColor({0, 0, 0});
			break;

		case InputMethod::SLIDER:
			input.mpSlider = new Slider(mpOptionsFont, SDL_Rect{MIN_SPACE, MIN_SPACE, mDimensions.w-2*MIN_SPACE, mDimensions.h-2*MIN_SPACE}, 0.0f);
			input.mpSlider->SetValue(1.0f);
			break;

		case InputMethod::CHOICES_ARRAY:
			input.mpChoicesArray = new ChoicesArray(SDL_Rect{MIN_SPACE, MIN_SPACE, mDimensions.w-2*MIN_SPACE, mDimensions.h-2*MIN_SPACE}, mDimensions.h-2*MIN_SPACE);
			break;

		case InputMethod::TICK:
			input.mpTickButton = new TickButton(SDL_Rect{MIN_SPACE, MIN_SPACE, mDimensions.h-MIN_SPACE*2, mDimensions.h-MIN_SPACE*2});
			break;

		case InputMethod::ACTION:
			input.mpActionButton = new ActionButton(SDL_Rect{MIN_SPACE, MIN_SPACE, (mDimensions.h-MIN_SPACE)*2, mDimensions.h-MIN_SPACE*2});
			break;
		
		default:
			ErrorPrint("Invalid input method: "+std::to_string(static_cast<int>(mInputMethod)));
			break;
	}
}



std::unique_ptr<Option::OptionCommands::t_commands_map> Option::OptionCommands::mCommandsMap;
//OPTION COMMANDS METHODS:

void Option::OptionCommands::LoadCommands(){
	mCommandsMap.reset(new t_commands_map());
	mCommandsMap->emplace("DefaultText", OptionCommands::SetDefaultText);
	mCommandsMap->emplace("SliderMin", OptionCommands::SetMinValue);
	mCommandsMap->emplace("SliderMax", OptionCommands::SetMaxValue);
	mCommandsMap->emplace("SliderDigits", OptionCommands::SetDecimalDigits);
	mCommandsMap->emplace("AddChoice", OptionCommands::AddChoiceToArray);
	mCommandsMap->emplace("OptionText", OptionCommands::SetOptionText);
	mCommandsMap->emplace("InitialValue", OptionCommands::SetInitialValue);
}

void Option::OptionCommands::UnloadCommands(){
	mCommandsMap.reset();
}

void Option::OptionCommands::HandleCommand(Option *pOption, std::string_view command){
	int barIndex = command.find_first_of('/');
	if(auto it = mCommandsMap->find(command.substr(0, barIndex)); it != mCommandsMap->end()){
		it->second(pOption, command.substr(barIndex+1));
	} else {
		OptionCommands::UnusableInfo(pOption, command);
	}
}

void Option::OptionCommands::SetInitialValue(Option *pOption, std::string_view nValue){
	switch(pOption->mInputMethod){
		case Option::InputMethod::TEXT_FIELD:
		case Option::InputMethod::HEX_TEXT_FIELD:
		case Option::InputMethod::WHOLE_TEXT_FIELD:
			pOption->input.mpTextField->SetText(nValue);
			break;
		case Option::InputMethod::SLIDER:{
			float nSliderValue;
			std::from_chars(nValue.data(), nValue.data()+nValue.size(), nSliderValue);
			pOption->input.mpSlider->SetValue(nSliderValue);
			break;
		}
		case Option::InputMethod::CHOICES_ARRAY:{
			float nChosen;
			std::from_chars(nValue.data(), nValue.data()+nValue.size(), nChosen);
			pOption->input.mpChoicesArray->UncheckedSetLastChosenOption(nChosen);
			break;
		}
		case Option::InputMethod::TICK:
			pOption->input.mpTickButton->SetValue(nValue.size() == 1 && nValue[0] == 'T');
			break;
		case Option::InputMethod::ACTION:
			DebugPrint("Input method ACTION can't have an initial value nor a value in general");
			break;
		default:
			ErrorPrint("Option doesn't have a valid input method, value: "+std::string(nValue));
			return;
	}
	pOption->mModified = true;
}
        
void Option::OptionCommands::SetOptionText(Option *pOption, std::string_view nOptionText){
	pOption->SetOptionText(std::string(nOptionText.data(), nOptionText.data()+nOptionText.size()).c_str());
}

void Option::OptionCommands::SetMinValue(Option *pOption, std::string_view nMin){
	if(pOption->mInputMethod != Option::InputMethod::SLIDER){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" is not a slider, inputMethod: "+std::to_string(static_cast<unsigned int>(pOption->mInputMethod)));
		return;
	}

	try{
		float minValue;
		std::from_chars(nMin.data(), nMin.data()+nMin.size(), minValue);
		pOption->input.mpSlider->SetMinValue(minValue);
	} catch(std::invalid_argument const &e){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" could not transform" + std::string(nMin.data(), nMin.data()+nMin.size()) + "into a float");
	}
}

void Option::OptionCommands::SetMaxValue(Option *pOption, std::string_view nMax){
	if(pOption->mInputMethod != Option::InputMethod::SLIDER){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" is not a slider, inputMethod: "+std::to_string(static_cast<unsigned int>(pOption->mInputMethod)));
		return;
	}

	try{
		float maxValue;
		std::from_chars(nMax.data(), nMax.data()+nMax.size(), maxValue);
		pOption->input.mpSlider->SetMaxValue(maxValue);
	} catch(std::invalid_argument const &e){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" could not transform" + std::string(nMax.data(), nMax.data()+nMax.size()) + "into a float");
	}
}

void Option::OptionCommands::SetDecimalDigits(Option *pOption, std::string_view nDigits){
	if(pOption->mInputMethod != Option::InputMethod::SLIDER){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" is not a slider, inputMethod: "+std::to_string(static_cast<unsigned int>(pOption->mInputMethod)));
		return;
	}

	try{
		int decimalDigits;
		std::from_chars(nDigits.data(), nDigits.data()+nDigits.size(), decimalDigits);
		pOption->input.mpSlider->SetDecimalPlaces(decimalDigits);
	} catch(std::invalid_argument const &e){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" could not transform" + std::string(nDigits.data(), nDigits.data()+nDigits.size()) + "into a float");
	}
}

void Option::OptionCommands::AddChoiceToArray(Option *pOption, std::string_view nDefaultText){
	if(pOption->mInputMethod != Option::InputMethod::CHOICES_ARRAY){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" is not a choice array, inputMethod: "+std::to_string(static_cast<unsigned int>(pOption->mInputMethod)));
		return;
	}

	pOption->input.mpChoicesArray->AddOption(std::string(nDefaultText.data(), nDefaultText.data()+nDefaultText.size()));
}

void Option::OptionCommands::SetDefaultText(Option *pOption, std::string_view nDefaultText){
	if(pOption->mInputMethod != Option::InputMethod::TEXT_FIELD && pOption->mInputMethod != Option::InputMethod::WHOLE_TEXT_FIELD && pOption->mInputMethod != Option::InputMethod::HEX_TEXT_FIELD){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" is not a textfield, inputMethod: "+std::to_string(static_cast<unsigned int>(pOption->mInputMethod)));
		return;
	}

	pOption->input.mpTextField->SetBlankText(nDefaultText);
}

void Option::OptionCommands::UnusableInfo(Option *pOption, std::string_view nUnusableInfo){
	ErrorPrint("id "+std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" found some garbage \'"+std::string(nUnusableInfo.data(), nUnusableInfo.data()+nUnusableInfo.size())+'\'');
}