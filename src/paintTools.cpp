#include "paintTools.hpp"
#include "renderLib.hpp"
#include "logger.hpp"
#include <iomanip>
#include <functional>

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
	if(SDL_HasIntersection(&previousViewport, &realDimensions) == SDL_TRUE){
		SDL_RenderSetViewport(pRenderer, &realDimensions);
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
	mFilledDimensions.w = (int)(mDimensions.w * (mValue-mMin)/(mMax-mMin));
	mTextField.dimensions.w = mDimensions.w;
}

void Slider::SetDimensions(SDL_Rect nDimensions){
	mDimensions = nDimensions;
	mFilledDimensions = nDimensions;
	mFilledDimensions.w = (int)(mDimensions.w * (mValue-mMin)/(mMax-mMin));
	mTextField.dimensions = mDimensions;
}

SDL_Rect Slider::GetDimensions(){
	return mDimensions;
}

void Slider::SetValue(float nValue){
	//We adjusti it so that it only conserves the most important digits
	nValue = std::round(std::pow(10, VALUE_DECIMAL_PLACES)*std::clamp(nValue, mMin, mMax))*std::pow(0.1f, VALUE_DECIMAL_PLACES);

	//Then we check if the value has changed in any way
	mHasChanged = (mValue != nValue);
	if(!mHasChanged){
		return;
	}
	mValue = nValue;

	//We set the filled dimensions width
	mFilledDimensions.w = (int)(mDimensions.w * (nValue-mMin)/(mMax-mMin));

	//We transform the value into a string and finally we get only the part we want (all digits except the decimal places that we do not want)
	//Even if before we set mValue's precision, the float still holds those decimal values (either as 0s or 9s), so we still have to truncate them
	std::string textValue = std::to_string(mValue);
	mTextField.SetText(textValue.substr(0, textValue.find('.')+(VALUE_DECIMAL_PLACES+1)));
}

void Slider::SetMinValue(float nMin){
	mMin = nMin;
	
	mFilledDimensions.w = (int)(mDimensions.w * (mValue-mMin)/(mMax-mMin));
}
void Slider::SetMaxValue(float nMax){
	mMax = nMax;

	mFilledDimensions.w = (int)(mDimensions.w * (mValue-mMin)/(mMax-mMin));
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
		SetValue(mValue + event->wheel.y*std::pow(0.1f, VALUE_DECIMAL_PLACES));
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
			
			return SetLastChosenOption(newChosen);
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

//POSITION PICKER BUTTON METHODS:

PositionPickerButton::PositionPickerButton(SDL_Texture *npTexture) : mpTexture(npTexture){}

SDL_Point PositionPickerButton::GetPositionPicked(bool *wasSelected){
    if(wasSelected) {
		*wasSelected = (mPickingState == PickingState::POSITION_SELECTED); 
		
		if(*wasSelected) mPickingState = PickingState::NONE;
	}
    return mMouseClick;
}

bool PositionPickerButton::HandleEvent(SDL_Event *event){
    if(event->type == SDL_MOUSEBUTTONDOWN){
		SDL_Point mousePos = {event->button.x, event->button.y};

		if(SDL_PointInRect(&mousePos, &dimensions)){
			mPickingState = PickingState::BUTTON_CLICKED;
            
            return true;

		} else if (mPickingState == PickingState::BUTTON_CLICKED){
			mPickingState = PickingState::POSITION_SELECTED;
            mMouseClick = {event->button.x, event->button.y};
            return true;
		}
	}

    return false;
}
void PositionPickerButton::Draw(SDL_Renderer *pRenderer){
    SDL_RenderCopy(pRenderer, mpTexture.get(), nullptr, &dimensions);
}

//PENCIL METHODS:

void Pencil::SetRadius(int nRadius){
	if(nRadius <= 1){
		nRadius = 1;
	}

	//If the radius hasn't changed, we don't need to update the preview or circle
	if(nRadius-1 != mRadius){
		mRadius = nRadius-1;
		UpdateCirclePixels();
		UpdatePreviewRects();
	}
}

int Pencil::GetRadius(){
	return mRadius+1;
}

void Pencil::SetHardness(float nHardness){
	mHardness = std::clamp(nHardness, 0.0f, 1.0f);
	//If the pencil is hard, then hardness has no effect on the pixels alphas
	if(mPencilType != PencilType::HARD) UpdateCircleAlphas();
}

float Pencil::GetHardness(){
	return mHardness;
}

void Pencil::SetAlphaCalculation(AlphaCalculation nAlphaCalculation){
	mAlphaCalculation = nAlphaCalculation;
	//If the pencil is hard, then alpha calculation mode has no effect on the pixels alphas
	if(mPencilType != PencilType::HARD) UpdateCircleAlphas();
}

void Pencil::SetPencilType(PencilType nPencilType){
	mPencilType = nPencilType;
	UpdateCircleAlphas();
}

std::vector<Pencil::DrawPoint> Pencil::ApplyOn(SDL_Point pixel, SDL_Rect *usedArea){
	std::vector<Pencil::DrawPoint> result(mCirclePixels);

	for(auto &drawPoint : result){
		drawPoint.pos.x += pixel.x;
		drawPoint.pos.y += pixel.y;
	}
	 
	if(usedArea){
		*usedArea = {pixel.x - mRadius, pixel.y - mRadius, 2*mRadius+1, 2*mRadius+1};
	}

	return result;
}

void Pencil::SetResolution(float nResolution){
	mRectsResolution = nResolution;
	UpdatePreviewRects();
}

void Pencil::DrawPreview(SDL_Point center, SDL_Renderer *pRenderer, SDL_Color previewColor){	
	SDL_SetRenderDrawColor(pRenderer, previewColor.r, previewColor.g, previewColor.b, previewColor.a);
	
	SDL_Rect *rects = new SDL_Rect[mPreviewRects.size()];

	for(int i = 0; i < mPreviewRects.size(); ++i){
		rects[i] = mPreviewRects[i];
		rects[i].x += center.x;
		rects[i].y += center.y;
	}
	
	SDL_RenderFillRects(pRenderer, rects, mPreviewRects.size());

	delete[] rects;
}

void Pencil::UpdateCirclePixels(){
	mCirclePixels.clear();

	SDL_Point center = {0, 0};
	if(mRadius >= 19){
		mCirclePixels.reserve(3.4*mRadius*mRadius);
	} else {
		int16_t sizes[19]{1, 9, 21, 45, 69, 97, 145, 185, 241, 293, 357, 437, 505, 593, 673, 773, 877, 981, 1093};
		mCirclePixels.reserve(sizes[mRadius]);
	}

	//Based on "Midpoint circle algorithm - Jesko's Method", but modified so that there are no repeating points
    int x = 1, y = mRadius;
	int t1 = mRadius/16, t2;
	FillHorizontalLine(center.y, center.x-y, center.x+y, center, mCirclePixels);
	while(y > x){
		FillHorizontalLine(center.y+x, center.x-y, center.x+y, center, mCirclePixels);
		FillHorizontalLine(center.y-x, center.x-y, center.x+y, center, mCirclePixels);

		x++;
		t1 += x;
		t2 = t1 - y;
		if(t2 >= 0){
			int xMinus = x-1;
			FillHorizontalLine(center.y+y, center.x-xMinus, center.x+xMinus, center, mCirclePixels);
			FillHorizontalLine(center.y-y, center.x-(x-1), center.x+(x-1), center, mCirclePixels);
			t1 = t2;
			y--;
		} 
	}
	if(y == x){
		FillHorizontalLine(center.y+x, center.x-y, center.x+y, center, mCirclePixels);
		FillHorizontalLine(center.y-x, center.x-y, center.x+y, center, mCirclePixels);
	}
}

void Pencil::UpdateCircleAlphas(){
	for(auto &pixel : mCirclePixels){
		SetPixelAlpha({0, 0}, pixel);
	}
}

void Pencil::SetPixelAlpha(const SDL_Point &center, DrawPoint &pixel){
	if(mPencilType ==  PencilType::HARD){
		pixel.alpha = SDL_ALPHA_OPAQUE;
	
	} else if(mPencilType ==  PencilType::SOFT){
		float centerDistance = sqrt(pow(pixel.pos.x-center.x, 2) + pow(pixel.pos.y-center.y, 2));
		float maxDistance = mRadius*mHardness;
		Uint8 alpha;
		
		if(centerDistance <= maxDistance){
			alpha = SDL_ALPHA_OPAQUE;
		}
		else switch(mAlphaCalculation){
			case AlphaCalculation::LINEAR: 
				alpha = (Uint8)(SDL_ALPHA_OPAQUE * (1-((centerDistance-maxDistance)/(1+mRadius-maxDistance))));
				break;
			case AlphaCalculation::QUADRATIC:
				alpha = (Uint8)(SDL_ALPHA_OPAQUE * (1-pow((centerDistance-maxDistance)/(1+mRadius-maxDistance), 2)));
				break;
			case AlphaCalculation::EXPONENTIAL:
				alpha = (Uint8)(SDL_ALPHA_OPAQUE * exp(-6*(centerDistance-maxDistance)/(1+mRadius-maxDistance)));
				break;
		}

		pixel.alpha = alpha;
	}
}

void Pencil::FillHorizontalLine(int y, int minX, int maxX, const SDL_Point &circleCenter, std::vector<DrawPoint> &points){
	for(int x = minX; x <= maxX; ++x){
		DrawPoint newPixel = {x, y, 0};
		SetPixelAlpha(circleCenter, newPixel);
	
		if(newPixel.alpha != 0) points.push_back(newPixel);
	}
}

void Pencil::UpdatePreviewRects(){
	mPreviewRects.clear();
	
	//Currently 0,0. When displaying, the mouse coordinates will be added
	SDL_Point center = {0, 0};
	
	float resolution = mRectsResolution;
	int radius = mRadius;

	if(resolution < 1.0f) {
		//This derives from the following:
		//FinalResolution = 1
		//OriginalResolution * (x^n) = FinalResolution
		//n = logBaseX(1/OriginalResolution)
		//FinalRadius = OriginalRadius / (x^n)
		//FinalRadius = OriginalRadius / (x^logBaseX(1/OriginalResolution))
		//FinalRadius = OriginalRadius / (1 / OriginalResolution)
		//FinalRadius = OriginalRadius * OriginalResolution
		radius = (int)(radius*resolution);
		
		resolution = 1.0f;
	}

	SDL_Rect auxiliar = {0, 0, 0, 0}; //Doesn't actually require initialization 
	int rectLength = 0;
	
	//Just an aproximation
	mPreviewRects.reserve(mRadius*2);

	//The method we use to round the coordinates, currently it seems like ceil produces the best looking results (aka with less gaps)
	const std::function<float(float)> ROUND = ceilf;

	auto addRects = [&center, &resolution, &ROUND](std::vector<SDL_Rect> &mPreviewRects, int rectLength, SDL_Rect &auxiliar){
		mPreviewRects.emplace_back(center.x + (int)ROUND(resolution * auxiliar.x),                    center.y + (int)ROUND(resolution * auxiliar.y),                    (int)ROUND(resolution * auxiliar.w), (int)ROUND(resolution * auxiliar.h));
		mPreviewRects.emplace_back(center.x + (int)ROUND(resolution * auxiliar.x),                    center.y - (int)ROUND(resolution * (auxiliar.y + auxiliar.h - 1)), (int)ROUND(resolution * auxiliar.w), (int)ROUND(resolution * auxiliar.h));
		mPreviewRects.emplace_back(center.x - (int)ROUND(resolution * (auxiliar.x + auxiliar.w - 1)), center.y + (int)ROUND(resolution * auxiliar.y),                    (int)ROUND(resolution * auxiliar.w), (int)ROUND(resolution * auxiliar.h));
		mPreviewRects.emplace_back(center.x - (int)ROUND(resolution * (auxiliar.x + auxiliar.w - 1)), center.y - (int)ROUND(resolution * (auxiliar.y + auxiliar.h - 1)), (int)ROUND(resolution * auxiliar.w), (int)ROUND(resolution * auxiliar.h));
		mPreviewRects.emplace_back(center.x + (int)ROUND(resolution * auxiliar.y),                    center.y + (int)ROUND(resolution * auxiliar.x),                    (int)ROUND(resolution * auxiliar.h), (int)ROUND(resolution * auxiliar.w));
		mPreviewRects.emplace_back(center.x + (int)ROUND(resolution * auxiliar.y),                    center.y - (int)ROUND(resolution * (auxiliar.x + auxiliar.w - 1)), (int)ROUND(resolution * auxiliar.h), (int)ROUND(resolution * auxiliar.w));
		mPreviewRects.emplace_back(center.x - (int)ROUND(resolution * (auxiliar.y + auxiliar.h - 1)), center.y + (int)ROUND(resolution * auxiliar.x),                    (int)ROUND(resolution * auxiliar.h), (int)ROUND(resolution * auxiliar.w));
		mPreviewRects.emplace_back(center.x - (int)ROUND(resolution * (auxiliar.y + auxiliar.h - 1)), center.y - (int)ROUND(resolution * (auxiliar.x + auxiliar.w - 1)), (int)ROUND(resolution * auxiliar.h), (int)ROUND(resolution * auxiliar.w));
	};

	int x = 1, y = radius;
	int t1 = radius/16, t2;
	
	mPreviewRects.emplace_back(center.x - (int)ROUND(resolution * radius), center.y, (int)ROUND(resolution), (int)ROUND(resolution));
	mPreviewRects.emplace_back(center.x + (int)ROUND(resolution * radius), center.y, (int)ROUND(resolution), (int)ROUND(resolution));
	mPreviewRects.emplace_back(center.x, center.y - (int)ROUND(resolution * radius), (int)ROUND(resolution), (int)ROUND(resolution));
	mPreviewRects.emplace_back(center.x, center.y + (int)ROUND(resolution * radius), (int)ROUND(resolution), (int)ROUND(resolution));
	
	while(y > x){
		x++;
		rectLength++;
		t1 += x;
		t2 = t1 - y;
		if(t2 >= 0){
			t1 = t2;
			y--;

			auxiliar.x = x-rectLength;
			auxiliar.y = 1+y;
			auxiliar.w = rectLength;
			auxiliar.h = 1;
			
			addRects(mPreviewRects, rectLength, auxiliar);

			rectLength = 0;
		} 
	}
	if(y == x){
		mPreviewRects.emplace_back(center.x + (int)ROUND(resolution * x), center.y + (int)ROUND(resolution * y), (int)ROUND(resolution), (int)ROUND(resolution));
		mPreviewRects.emplace_back(center.x + (int)ROUND(resolution * x), center.y - (int)ROUND(resolution * y), (int)ROUND(resolution), (int)ROUND(resolution));
		mPreviewRects.emplace_back(center.x - (int)ROUND(resolution * x), center.y + (int)ROUND(resolution * y), (int)ROUND(resolution), (int)ROUND(resolution));
		mPreviewRects.emplace_back(center.x - (int)ROUND(resolution * x), center.y - (int)ROUND(resolution * y), (int)ROUND(resolution), (int)ROUND(resolution));
	}
}

//PENCIL MODIFIER METHODS:

PencilModifier::PencilModifier(Pencil &nModifyPencil, SDL_Rect nDimensions) : 
	mModifyPencil(nModifyPencil), dimensions(nDimensions){}

void PencilModifier::SetModifiedPencil(Pencil &nModifyPencil){
	mModifyPencil = nModifyPencil;
}

bool PencilModifier::HandleEvent(SDL_Event *event){
	//if(event->type == )

	//After local handling, call HandleEvent on all instances holded
	return false;
}

void PencilModifier::Update(float deltaTime){

}

void PencilModifier::Draw(SDL_Renderer* pRenderer){
	DrawRect(pRenderer, dimensions, {0, 0, 0, 255});
}

//MUTABLE TEXTURE METHODS:

MutableTexture::MutableTexture(SDL_Renderer *pRenderer, int width, int height, SDL_Color fillColor){
	mpSurface.reset(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888));
    mpTexture.reset(SDL_CreateTexture(pRenderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, width, height));

	Clear(fillColor);
}

MutableTexture::MutableTexture(SDL_Renderer *pRenderer, const char *pImage){
	SDL_Surface *loaded = IMG_Load(pImage);
    
	mpSurface.reset(SDL_ConvertSurfaceFormat(loaded, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888, 0));
    mpTexture.reset(SDL_CreateTexture(pRenderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, loaded->w, loaded->h));
	
    SDL_FreeSurface(loaded);
	
	UpdateWholeTexture();
}

SDL_Color MutableTexture::GetPixelColor(SDL_Point pixel, bool *validValue){
	
	if(IsPixelOutsideImage(pixel)){
		if(validValue) *validValue = false;
		return {0,0,0,0};
	} else {
		if(validValue) *validValue = true;
	}

	SDL_Color pixelColor;
	SDL_GetRGBA(*UnsafeGetPixel(pixel), mpSurface->format, &pixelColor.r, &pixelColor.g, &pixelColor.b, &pixelColor.a);
	return pixelColor;
}

void MutableTexture::Clear(const SDL_Color &clearColor){
	SDL_Rect surfaceRect {0, 0, mpSurface->w, mpSurface->h};
	SDL_FillRect(mpSurface.get(), &surfaceRect, SDL_MapRGBA(mpSurface->format, clearColor.r, clearColor.g, clearColor.b, 255));

	UpdateWholeTexture();
}

void MutableTexture::SetPixel(SDL_Point pixel, const SDL_Color &color){
    if(IsPixelOutsideImage(pixel)) {
        ErrorPrint("pixel has an invalid index");
        return;
    }

	//It is safe to call UnsafeSetPixel, since we already locked the surface and checked that the localPixel does exist
    SetPixelUnsafe(pixel, color);
}

void MutableTexture::SetPixels(std::span<SDL_Point> pixels, const SDL_Color &color){
	for(const auto &pixel : pixels){
		if(IsPixelOutsideImage(pixel)){
			continue;
		}
		SetPixelUnsafe(pixel, color);
	}
}

void MutableTexture::SetDrawnPixels(std::span<Pencil::DrawPoint> pixels, const SDL_Color &color){
	SDL_Color aux;
	for(const auto &pixel : pixels){
		if(IsPixelOutsideImage(pixel.pos)){
			continue;
		}
		SDL_GetRGBA(*UnsafeGetPixel(pixel.pos), mpSurface->format, &aux.r, &aux.g, &aux.b, &aux.a);
		float bsA = aux.a/255.0f, bsR = aux.r/255.0f, bsG = aux.g/255.0f, bsB = aux.b/255.0f;
		float blA = pixel.alpha/255.0f, blR = color.r/255.0f, blG = color.g/255.0f, blB = color.b/255.0f;
		aux.a = SDL_ALPHA_OPAQUE;
		aux.r = SDL_ALPHA_OPAQUE*(blR*blA+bsR*(1-blA));
		aux.g = SDL_ALPHA_OPAQUE*(blG*blA+bsG*(1-blA));
		aux.b = SDL_ALPHA_OPAQUE*(blB*blA+bsB*(1-blA));
		SetPixelUnsafe(pixel.pos, aux);
	}
}

void MutableTexture::SetPixelUnsafe(SDL_Point pixel, const SDL_Color &color){
	*UnsafeGetPixel(pixel) = SDL_MapRGBA(mpSurface->format, color.r, color.g, color.b, color.a);

	mChangedPixels.push_back(pixel);
}

void MutableTexture::SetPixelsUnsafe(std::span<SDL_Point> pixels, const SDL_Color &color){
	for(const auto &pixel : pixels){
		*UnsafeGetPixel(pixel) = SDL_MapRGBA(mpSurface->format, color.r, color.g, color.b, color.a);
	}

	mChangedPixels.insert(mChangedPixels.end(), pixels.begin(), pixels.end());
}

void MutableTexture::SetDrawnPixelsUnsafe(std::span<Pencil::DrawPoint> pixels, const SDL_Color &color){
	SDL_Color aux;

	for(const auto &pixel : pixels){
		SDL_GetRGBA(*UnsafeGetPixel(pixel.pos), mpSurface->format, &aux.r, &aux.g, &aux.b, &aux.a);
		float bsA = aux.a/255.0f, bsR = aux.r/255.0f, bsG = aux.g/255.0f, bsB = aux.b/255.0f;
		float blA = pixel.alpha/255.0f, blR = color.r/255.0f, blG = color.g/255.0f, blB = color.b/255.0f;
		aux.a = SDL_ALPHA_OPAQUE;
		aux.r = SDL_ALPHA_OPAQUE*(blR*blA+bsR*(1-blA));
		aux.g = SDL_ALPHA_OPAQUE*(blG*blA+bsG*(1-blA));
		aux.b = SDL_ALPHA_OPAQUE*(blB*blA+bsB*(1-blA));
		
		SetPixelUnsafe(pixel.pos, aux);
	}

}

void MutableTexture::UpdateTexture(){
	if(mChangedPixels.empty()) return;

	SDL_Surface *texturesSurface;
	SDL_Rect changedRect = GetChangesRect();

	SDL_LockTextureToSurface(mpTexture.get(), &changedRect, &texturesSurface);
	
	SDL_BlitSurface(mpSurface.get(), &changedRect, texturesSurface, nullptr);
	
	SDL_UnlockTexture(mpTexture.get());

	mChangedPixels.clear();
}

void MutableTexture::DrawIntoRenderer(SDL_Renderer *pRenderer, const SDL_Rect &dimensions){
	SDL_RenderCopy(pRenderer, mpTexture.get(), nullptr, &dimensions);
}

bool MutableTexture::Save(const char *pSavePath){
	if(IMG_SavePNG(mpSurface.get(), pSavePath)){
		ErrorPrint("Couldn't save image in file "+std::string(pSavePath));
		return true;
	}
	return false;
}

int MutableTexture::GetWidth(){
	return mpSurface->w;
}

int MutableTexture::GetHeight(){
	return mpSurface->h;
}

void MutableTexture::UpdateWholeTexture(){
	SDL_Surface *texturesSurface;
	SDL_LockTextureToSurface(mpTexture.get(), nullptr, &texturesSurface);
	
	//Makes sure that the texture gets updated with the exact info (no blending involved)
	SDL_SetSurfaceBlendMode(mpSurface.get(), SDL_BLENDMODE_NONE);
	SDL_BlitSurface(mpSurface.get(), nullptr, texturesSurface, nullptr);

	SDL_UnlockTexture(mpTexture.get());

	SDL_SetTextureBlendMode(mpTexture.get(), SDL_BLENDMODE_BLEND);
	
	mChangedPixels.clear();
}

SDL_Rect MutableTexture::GetChangesRect(){
	if(mChangedPixels.size() == 1){
		return {mChangedPixels[0].x, mChangedPixels[0].y, 1, 1};
	}

	int smallestX = INT_MAX, smallestY = INT_MAX, biggestX = INT_MIN, biggestY = INT_MIN;

	for(const auto &pixel : mChangedPixels){
		
		if(pixel.x > biggestX){
			biggestX = pixel.x;
		}
		if(pixel.x < smallestX){
			smallestX = pixel.x;
		}

		if(pixel.y > biggestY){
			biggestY = pixel.y;
		}
		if(pixel.y < smallestY){
			smallestY = pixel.y;
		}
	}

	return {smallestX, smallestY, 1+biggestX-smallestX, 1+biggestY-smallestY};
}

//SURFACE CANVAS METHODS:

Canvas::Canvas(SDL_Renderer *pRenderer, int nWidth, int nHeight) : 
	mpImage(new MutableTexture(pRenderer, nWidth, nHeight)){

	mDimensions = {0, 0, nWidth, nHeight};
}

Canvas::Canvas(SDL_Renderer *pRenderer, const char *pLoadFile) : mpImage(new MutableTexture(pRenderer, pLoadFile)){
	mDimensions = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};
}

Canvas::~Canvas(){
	if(saveOnDestroy) Save();
}

void Canvas::Resize(SDL_Renderer *pRenderer, int nWidth, int nHeight){
	mpImage.reset(new MutableTexture(pRenderer, nWidth, nHeight));
	mDimensions = {0, 0, nWidth, nHeight};
}
void Canvas::OpenFile(SDL_Renderer *pRenderer, const char *pLoadFile){
	mpImage.reset(new MutableTexture(pRenderer, pLoadFile));
	mDimensions = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};
}

SDL_Color Canvas::GetColor(){
	return mDrawColor;
}

void Canvas::SetColor(SDL_Color nDrawColor){
	mDrawColor = nDrawColor;
}

void Canvas::DrawPixel(SDL_Point localPixel){
	mLastMousePixel = localPixel;

	SDL_Rect usedArea, imageRect = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};

	std::vector<Pencil::DrawPoint> pixelsToDraw = pencil.ApplyOn(localPixel, &usedArea);

	if(!IsRectCompletelyInsideRect(usedArea, imageRect)){
		
		//If the rects do not even intersect, then there's no need to try drawing any pixel
		if(!SDL_HasIntersection(&usedArea, &imageRect)){
			return;
		}
		
		mpImage->SetDrawnPixels(pixelsToDraw, mDrawColor);
	}
	else {
		
		//We call an unsafe method since we know all the pixels fall inside the image
		mpImage->SetDrawnPixelsUnsafe(pixelsToDraw, mDrawColor);
	}

}

void Canvas::DrawPixels(const std::vector<SDL_Point> &localPixels){
	SDL_Rect usedArea, imageRect = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};
		
    mLastMousePixel = localPixels.back();

	//TODO: make faster (parallelize?)
	for(const auto& pixel : localPixels){
		std::vector<Pencil::DrawPoint> pixelsToDraw = pencil.ApplyOn(pixel, &usedArea);
	
		if(!IsRectCompletelyInsideRect(usedArea, imageRect)){
			
			//If the rects do not even intersect, then there's no need to try drawing any pixel
			if(!SDL_HasIntersection(&usedArea, &imageRect)){
				continue;
			}

			mpImage->SetDrawnPixels(pixelsToDraw, mDrawColor);
		}
		else {
			//We call an unsafe method since we know all the pixels fall inside the image
			mpImage->SetDrawnPixelsUnsafe(pixelsToDraw, mDrawColor);
		}

	}
}

void Canvas::Clear(std::optional<SDL_Color> clearColor){
	if (clearColor.has_value()) {
		mpImage->Clear(clearColor.value());
	} else {
		mpImage->Clear(mDrawColor);
	}
}

void Canvas::SetSavePath(const char *nSavePath){
	mSavePath = nSavePath;
}

void Canvas::SetOffset(int offsetX, int offsetY){
	mDimensions.x = offsetX;
	mDimensions.y = offsetY;
}

void Canvas::SetResolution(float nResolution){
	//if(nResolution < M_MIN_RESOLUTION || nResolution > M_MAX_RESOLUTION) return;
	mResolution = std::clamp(nResolution, M_MIN_RESOLUTION, M_MAX_RESOLUTION);
	pencil.SetResolution(mResolution);

	int nWidth = mpImage->GetWidth()*mResolution, nHeight = mpImage->GetHeight()*mResolution;

	mDimensions.x += (mDimensions.w-nWidth)/2;
	mDimensions.y += (mDimensions.h-nHeight)/2;
	mDimensions.w = nWidth;
	mDimensions.h = nHeight;
}

void Canvas::HandleEvent(SDL_Event *event){
	if(event->type == SDL_MOUSEBUTTONDOWN){
        //Check the mouse is in the viewport
        SDL_Point mousePos = {event->button.x, event->button.y};
		if(!SDL_PointInRect(&mousePos, &viewport)) return;

		SDL_Point pixel = GetPointCell({mousePos.x-(mDimensions.x+viewport.x), mousePos.y-(mDimensions.y+viewport.y)}, mResolution);

		DrawPixel(pixel);

		mHolded = true;
	}
	else if (mHolded && event->type == SDL_MOUSEMOTION){
        SDL_Point mousePos = {event->button.x, event->button.y};
		SDL_Point pixel = GetPointCell({mousePos.x-(mDimensions.x+viewport.x), mousePos.y-(mDimensions.y+viewport.y)}, mResolution);

        //Check the mouse is in the viewport
		if(!SDL_PointInRect(&mousePos, &viewport)){
			//We still set mLastMousePixel
			mLastMousePixel = pixel;
			return;
		}

	
		//Check the pixel wasn't the last one modified
		if(mLastMousePixel == pixel) return;
		
		//If the last pixel where the mouse was registered and the current pixel are separated by more than 1 unit in any axis, we also set the pixels in between
		if(std::abs(pixel.x-mLastMousePixel.x) > 1 || std::abs(pixel.y-mLastMousePixel.y) > 1){
			DrawPixels(GetPointsInSegment(mLastMousePixel, pixel));
		} else {
			DrawPixel(pixel);
		}
	}
	else if (event->type == SDL_MOUSEBUTTONUP){
		mHolded = false;
	}
	else if(event->type == SDL_KEYDOWN){
		//We don't want to handle a key down if the text input is active, because that means a textfield is using the input
		if(SDL_IsTextInputActive()) return;

		switch(event->key.keysym.sym){
			case SDLK_a: mDimensions.x++; break;
			case SDLK_d: mDimensions.x--; break;
			case SDLK_w: mDimensions.y++; break;
			case SDLK_s: mDimensions.y--; break;
			case SDLK_e: SetResolution(mResolution+10.0f*M_MIN_RESOLUTION); break;
			case SDLK_q: SetResolution(mResolution-10.0f*M_MIN_RESOLUTION); break;
			case SDLK_p: Save(); break;
		}
	}
	else if (event->type == SDL_MOUSEWHEEL){
		//TODO: Call change function assigned, this makes it so you can customize what value does the mouse wheel change
		//It should also check the wheel is used inside the canvas viewport (no need to be in the canvas exactly), as otherwise it should operate on the hovered text if any

		//No need to clamp the value, as SetHardness already does that
		pencil.SetHardness(pencil.GetHardness() + event->wheel.y*0.1f);
	}
}

void Canvas::Update(float deltaTime){
	mpImage->UpdateTexture();
}

void Canvas::DrawIntoRenderer(SDL_Renderer *pRenderer){
	SDL_SetRenderDrawColor(pRenderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRect(pRenderer, &viewport);

	SDL_RenderSetViewport(pRenderer, &viewport);
	
	//Gives a shade to the image, so it is easily distinguishable from the background, works better with birght colors
	for(int maxBorder = 20, border = maxBorder; border >= 1; --border){
		SDL_Rect imageShade = {mDimensions.x - 2*border, mDimensions.y - 2*border, mDimensions.w + 4*border, mDimensions.h + 4*border};

		SDL_Color shadeColor = {(Uint8)(backgroundColor.r*border/(float)maxBorder),
								(Uint8)(backgroundColor.g*border/(float)maxBorder), 
								(Uint8)(backgroundColor.b*border/(float)maxBorder)};

		SDL_SetRenderDrawColor(pRenderer, shadeColor.r, shadeColor.g, shadeColor.b, SDL_ALPHA_OPAQUE);
		SDL_RenderFillRect(pRenderer, &imageShade);
	}

	mpImage->DrawIntoRenderer(pRenderer, mDimensions);

	if(!mHolded || pencil.GetRadius() > 4){
		SDL_Point mousePosition;
		SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

		SDL_Rect intersectionRect;
		if(SDL_IntersectRect(&mDimensions, &viewport, &intersectionRect) == SDL_FALSE) intersectionRect = mDimensions;
		SDL_RenderSetViewport(pRenderer, &intersectionRect);

		//We find the middle pixel
		SDL_Point pixel = GetPointCell({mousePosition.x-(mDimensions.x), mousePosition.y-(mDimensions.y)}, mResolution);
		
		SDL_Color previewColor = pencilDisplayMainColor;
		bool validColor = true;
		SDL_Color pixelColor = mpImage->GetPixelColor(pixel, &validColor);
		if(validColor && pixelColor.r + pixelColor.g + pixelColor.b <= 127*3){
			previewColor = pencilDisplayAlternateColor;
		}
		
		pixel.x *= mResolution; pixel.y *= mResolution;
		pixel.x -= intersectionRect.x-mDimensions.x; pixel.y -= intersectionRect.y-mDimensions.y;
		
		pencil.DrawPreview(pixel, pRenderer, previewColor);
	}

	SDL_RenderSetViewport(pRenderer, nullptr);
}

void Canvas::Save(){
	if(mSavePath.empty()){
		return;
	}

	DebugPrint("About to save "+mSavePath);
	
	if(!mpImage->Save(mSavePath.c_str())){
		DebugPrint("Saved "+mSavePath);
	}
}

void Canvas::CenterInViewport(){
	mDimensions.x = (viewport.w-mDimensions.w)/2;
	mDimensions.y = (viewport.h-mDimensions.h)/2;
}

int Canvas::GetResolution(){
	return mResolution;
}

SDL_Point Canvas::GetImageSize(){
	return {mpImage->GetWidth(), mpImage->GetHeight()};
}

SDL_Point Canvas::GetGlobalPosition(){
	return {mDimensions.x+viewport.x, mDimensions.y+viewport.y};
}

MutableTexture *Canvas::GetImage(){
	return mpImage.get();
}