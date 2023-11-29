#include "engineInternals.hpp"
#include "renderLib.hpp"
#include "logger.hpp"
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <charconv>

void MainLoop(AppManager &appWindow, std::span<char*> args){
	bool keepRunning = true;
	SDL_Event ev;
	Uint64 lastUpdate = 0, currentUpdate = SDL_GetPerformanceCounter();
	float deltaTime;

	if(args.size() == 2) appWindow.AddImage(args[1]);

	while(keepRunning){
		while(SDL_PollEvent(&ev)){
			if((ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE) || (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)){
				keepRunning = false;
				break;
			}

			appWindow.HandleEvent(&ev);
		}
		
		lastUpdate = currentUpdate;
		currentUpdate = SDL_GetPerformanceCounter();
		deltaTime = ((currentUpdate - lastUpdate) / (float)SDL_GetPerformanceFrequency());

		appWindow.Update(deltaTime);

		appWindow.Draw();
	}
}



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
			if (InternalWindow::MakeEventRelativeToRect(mDimensions, event->button.x, event->button.y, originalMousePosition, eventMouseX, eventMouseY)) return false;
			else wasOptionClicked = true; //We don't need other options handling the button down if this one already did
			break;
		case SDL_MOUSEBUTTONUP:
			//We want to know when the mouse is up, even if it is outside the option
			InternalWindow::MakeEventRelativeToRect(mDimensions, event->motion.x, event->motion.y, originalMousePosition, eventMouseX, eventMouseY, false);
			break;
		case SDL_MOUSEMOTION:
			//We want to keep track of the mouse motion, even if it is outside the option
			InternalWindow::MakeEventRelativeToRect(mDimensions, event->motion.x, event->motion.y, originalMousePosition, eventMouseX, eventMouseY, false);
			break;
		case SDL_MOUSEWHEEL:
			if (InternalWindow::MakeEventRelativeToRect(mDimensions, event->wheel.mouseX, event->wheel.mouseY, originalMousePosition, eventMouseX, eventMouseY)) return false;
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
	
	mOptionText.reset(new ConstantText(pNewText, AppManager::GetAppFont()));
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
			input.mpTextField = new TextField(AppManager::GetAppFont(), TextField::TextFormat::NONE);
			input.mpTextField->dimensions = {MIN_SPACE, MIN_SPACE, mDimensions.w-MIN_SPACE*2, mDimensions.h-MIN_SPACE*2};
			input.mpTextField->SetColor({0, 0, 0});
			break;
		case InputMethod::HEX_TEXT_FIELD:
			input.mpTextField = new TextField(AppManager::GetAppFont(), TextField::TextFormat::HEX);
			input.mpTextField->dimensions = {MIN_SPACE, MIN_SPACE, mDimensions.w-MIN_SPACE*2, mDimensions.h-MIN_SPACE*2};
			input.mpTextField->SetColor({0, 0, 0});
			break;

		case InputMethod::WHOLE_TEXT_FIELD:
			input.mpTextField = new TextField(AppManager::GetAppFont(), TextField::TextFormat::WHOLE_POSITIVE);
			input.mpTextField->dimensions = {MIN_SPACE, MIN_SPACE, mDimensions.w-MIN_SPACE*2, mDimensions.h-MIN_SPACE*2};
			input.mpTextField->SetColor({0, 0, 0});
			break;

		case InputMethod::SLIDER:
			input.mpSlider = new Slider(AppManager::GetAppFont(), SDL_Rect{MIN_SPACE, MIN_SPACE, mDimensions.w-2*MIN_SPACE, mDimensions.h-2*MIN_SPACE}, 0.0f);
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

//INTERNAL WINDOW METHODS:

InternalWindow::InternalWindow(SDL_Point optionsSize, InitializationData nData, SDL_Renderer *pRenderer) : M_WINDOW_NAME(nData.windowName), mpIcon(LoadTexture(("Sprites/"+nData.windowName+".png").c_str(), pRenderer)){

	Option::OptionCommands::LoadCommands();

	mOptions.reserve(nData.dataAmount);

	int firstNewLine = nData.optionsInfo.find_first_of('\n');
	int textWidth = ProcessWindowInfo(nData.optionsInfo.substr(0, firstNewLine));

	int y = 0;
	for(size_t i = firstNewLine+1, nextI; i < nData.optionsInfo.size(); i = nextI + 1){
		nextI = nData.optionsInfo.find_first_of('\n', i);
		
		if(nextI == std::string::npos){
			//If the info wasn't ended in a new line, we just grab whatever is at the end (even if it's just unusable)
			nextI = nData.optionsInfo.size()-1;
		}

		mOptions.emplace_back(new Option(textWidth, SDL_Rect{0, y, optionsSize.x-mInnerBorder*2, Option::MIN_SPACE*2+optionsSize.y-mInnerBorder*2}, nData.optionsInfo.substr(i, nextI-i)));	
		
		y += mOptions.back()->mDimensions.h;
	}

	Option::OptionCommands::UnloadCommands();
	
	UpdateContentDimensions();
}

void InternalWindow::SetPosition(int x, int y){
	SDL_Point mainWindowSize; AppManager::GetWindowSize(mainWindowSize.x, mainWindowSize.y);
	mDimensions.x = std::clamp(x, 0, mainWindowSize.x - mDimensions.w);
	mDimensions.y = std::clamp(y, AppManager::GetMinimumWindowY(), mainWindowSize.y - mDimensions.h);
	UpdateContentDimensions();
}

void InternalWindow::SetSize(int w, int h){
	mDimensions.w = std::max(w, mMinGap);
	mDimensions.h = std::max(h, mMinGap);
	UpdateContentDimensions();
}

void InternalWindow::SetDimensions(const SDL_Rect &nDimensions){
	SetSize(nDimensions.w, nDimensions.h);
	SetPosition(nDimensions.x, nDimensions.y);
}

void InternalWindow::AddTemporalData(OptionInfo *newData){
	//TODO: make it so that sliders don't transmit their value until mouse up? (may have problems for previsualization)
	
	for(auto &current : mTemporalData){
		if(current->optionID == newData->optionID){
			current->SetTo(*newData);
			return;
		}
	}

	mTemporalData.push_back(std::make_shared<OptionInfo>());
	mTemporalData.back()->SetTo(*newData);
}

std::vector<std::shared_ptr<OptionInfo>> &InternalWindow::GetTemporalData(){
	return mTemporalData;
}

const std::string_view InternalWindow::GetName(){
	return M_WINDOW_NAME;
}

void InternalWindow::Minimize(){
	if(!mMinimized){
		mPreMiniSize.x = mDimensions.w;
		mPreMiniSize.y = mDimensions.h;
		mDimensions.w = M_MIN_SIZE.x;
		mDimensions.h = M_MIN_SIZE.y;
	} else {
		mDimensions.w = mPreMiniSize.x;
		mDimensions.h = mPreMiniSize.y;
		UpdateContentDimensions();
	}

	mMinimized = !mMinimized;
	mDraggedState = DraggedState::NONE;
}

bool InternalWindow::HandleEvent(SDL_Event *event){
	SDL_Point originalMousePosition;
	int *eventMouseX = nullptr, *eventMouseY = nullptr;
	
	bool wasEventHandled = false;
	bool wasWindowClicked = false;

	switch (event->type)
	{
		case SDL_KEYDOWN:
			if(event->key.keysym.sym == SDLK_LCTRL) mResizeOnDrag = true;
			break;
		case SDL_KEYUP:
			if(event->key.keysym.sym == SDLK_LCTRL) mResizeOnDrag = false;
			break;

		case SDL_MOUSEBUTTONDOWN:
			if(mMinimized){
				SDL_Point mousePos = {event->button.x, event->button.y};
				if(SDL_PointInRect(&mousePos, &mDimensions) == SDL_TRUE){
					mDraggedState = DraggedState::MOVEMENT;
					mDraggedData.x = mDimensions.x - event->button.x;
					mDraggedData.y = mDimensions.y - event->button.y;
					return true;
				}
				return false;
			}
			else if(Border clickedBorder; PointInsideInnerBorder({event->button.x, event->button.y}, &clickedBorder)) {
				wasEventHandled = true;
				if(!mResizeOnDrag){
					mDraggedState = DraggedState::MOVEMENT;
					mDraggedData.x = mDimensions.x - event->button.x;
					mDraggedData.y = mDimensions.y - event->button.y;
				}
				else {
					switch(clickedBorder){
						case Border::TOP:
							mDraggedState = DraggedState::RESIZE_TOP;
							break;
						case Border::LEFT:
							mDraggedState = DraggedState::RESIZE_LEFT;
							break;
						case Border::BOTTOM:
							mDraggedState = DraggedState::RESIZE_BOTTOM;
							break;
						case Border::RIGHT:
							mDraggedState = DraggedState::RESIZE_RIGHT;
							break;
					}
					mDraggedData = mDimensions;
				}
				return true;
			}

			//In case of mouse down we clearly want to ensure it occurs inside the window
			if(MakeEventRelativeToRect(mContentDimensions, event->button.x, event->button.y, originalMousePosition, eventMouseX, eventMouseY)) return false;
			else wasWindowClicked = true;
			break;
		
		case SDL_MOUSEMOTION:{
			wasEventHandled = true;
			switch(mDraggedState){
				case DraggedState::MOVEMENT:
					SetPosition(mDraggedData.x + event->button.x, mDraggedData.y + event->button.y);
					break;
				
				case DraggedState::RESIZE_TOP:
					mDimensions.y = std::min(event->button.y, mDraggedData.y + mDraggedData.h - mMinGap);
					mDimensions.h = mDraggedData.h + mDraggedData.y - mDimensions.y;
					UpdateContentDimensions();
					break;

				case DraggedState::RESIZE_LEFT:
					mDimensions.x = std::min(event->button.x, mDraggedData.x + mDraggedData.w - mMinGap);
					mDimensions.w = mDraggedData.w + mDraggedData.x - mDimensions.x;
					UpdateContentDimensions();
					break;

				case DraggedState::RESIZE_BOTTOM:
					mDimensions.h = std::max(event->button.y - mDimensions.y, mMinGap);
					UpdateContentDimensions();
					break;

				case DraggedState::RESIZE_RIGHT:
					mDimensions.w = std::max(event->button.x - mDimensions.x, mMinGap);
					UpdateContentDimensions();
					break;

				case DraggedState::NONE:
					wasEventHandled = false;
					break;
			}

			if(wasEventHandled) return true;
			else if(mMinimized) return false;

			//In case of mouse motion we do not care if it occurs outside of the window, as something like a slider could be dragged
			MakeEventRelativeToRect(mContentDimensions, event->motion.x, event->motion.y, originalMousePosition, eventMouseX, eventMouseY, false);
			break;
		}

		case SDL_MOUSEBUTTONUP:
			mDraggedState = DraggedState::NONE;

			if(mMinimized) return false;
			
			//In case of mouse up we do not care if it occurs outside of the window, as something like a slider could be dragged
			MakeEventRelativeToRect(mContentDimensions, event->button.x, event->button.y, originalMousePosition, eventMouseX, eventMouseY, false);
			break;

		case SDL_MOUSEWHEEL:

			if(mMinimized) return false;

			//The mouse wheel we want to ensure it occurs inside the window, otherwise it should be handled by another window or the canvas itself
			if(MakeEventRelativeToRect(mContentDimensions, event->wheel.mouseX, event->wheel.mouseY, originalMousePosition, eventMouseX, eventMouseY)) return false;
			break;
	}

	if(mMinimized){
		return wasEventHandled || wasWindowClicked;
	}
	//Options handle the event
	//We do not check 'wasEventHandled' before as it shouldn't be true if the flow has reached here
	for(auto &option : mOptions){
		if(option->HandleEvent(event)){
			wasEventHandled = true;
			break;
		}
	}

	//Finally, if the coordinates of the event could have been changed, they are set back to its initial values
	if(eventMouseX && eventMouseY){
		*eventMouseX = originalMousePosition.x;
		*eventMouseY = originalMousePosition.y;
	}
	
	return wasEventHandled || wasWindowClicked;
}

void InternalWindow::Update(float deltaTime){

}

void InternalWindow::Draw(SDL_Renderer *pRenderer){
	if(mMinimized){
		if(mpIcon.get() != nullptr) DisplayTexture(pRenderer, mpIcon.get(), &mDimensions);
		else 						FillRect(pRenderer, mDimensions, 100, 100, 100);
	} else {
		FillRect(pRenderer, mDimensions, 100, 100, 100); // border

		FillRect(pRenderer, mContentDimensions, 200, 200, 200); 

		SDL_RenderSetViewport(pRenderer, &mContentDimensions);

		for(auto &option : mOptions){
			option->Draw(pRenderer);
		};

		SDL_RenderSetViewport(pRenderer, nullptr);
	}
}

std::vector<std::shared_ptr<OptionInfo>> InternalWindow::GetData(){
	std::vector<std::shared_ptr<OptionInfo>> data;
	OptionInfo *auxiliar;

	for(auto &option : mOptions){
		auxiliar = option->GetData();
		if(!auxiliar->IsInvalid()) data.emplace_back(auxiliar);
		else delete auxiliar;
	};
	
	return data;
}

bool InternalWindow::MakeEventRelativeToRect(const SDL_Rect &dimensions, int &eventX, int &eventY, SDL_Point &originalMousePosition, bool resetIfUnable){
	//First we set the mouse original position
	originalMousePosition.x = eventX;
	originalMousePosition.y = eventY;

	//Then we change the coordinates and check if it falls inside the window
	eventX -= dimensions.x;
	eventY -= dimensions.y;

	if(eventX >= 0 && eventX < dimensions.w && eventY >= 0 && eventY < dimensions.h){
		return false;
	}

	//If it doesn't we set the values back and return true, as it falls outside the rect
	if(resetIfUnable){
		eventX = originalMousePosition.x;
		eventY = originalMousePosition.y;
	}
	return true;
}

bool InternalWindow::MakeEventRelativeToRect(const SDL_Rect &dimensions, int &eventX, int &eventY, SDL_Point &originalMousePosition, int *&pEventX, int *&pEventY, bool resetIfUnable){
	//First we set the mouse original position and the pointers 
	originalMousePosition.x = eventX;
	originalMousePosition.y = eventY;
	
	pEventX = &eventX;
	pEventY = &eventY;

	//Then we change the coordinates and check if it falls inside the window
	eventX -= dimensions.x;
	eventY -= dimensions.y;
	
	if(eventX >= 0 && eventX < dimensions.w && eventY >= 0 && eventY < dimensions.h){
		return false;
	}

	//If it doesn't we set the values back and return true, as it falls outside the rect
	if(resetIfUnable){
		eventX = originalMousePosition.x;
		eventY = originalMousePosition.y;
	}
	return true;
}

int InternalWindow::ProcessWindowInfo(std::string_view info){
	int i = 0, dimension;
	
	for(size_t firstIndex = 0, lastIndex; firstIndex < info.size(); firstIndex = lastIndex+1){
		lastIndex = info.find_first_of('_', firstIndex);

		if(lastIndex == std::string::npos){
			//If the info wasn't ended in a underscore, we just grab whatever is at the end (even if it's just unusable)
			lastIndex = info.size()-1;
		}

		auto result = std::from_chars(info.data()+firstIndex, info.data()+lastIndex, dimension);

		if(result.ec == std::errc::invalid_argument){
			ErrorPrint("Could not transform: \'" + std::string(info.data()+firstIndex, info.data()+lastIndex) + "\' into a number");
		}
		else switch(i){
			case 0: mDimensions.x = dimension; break;
			case 1: mDimensions.y = dimension + AppManager::GetMinimumWindowY(); break;
			case 2: mDimensions.w = dimension; break;
			case 3: mDimensions.h = dimension; break;
			case 4: return dimension; //The maximum width of the options' text
			default: ErrorPrint("Too much data in the first line of window info"); break;
		}

		i++;
	}

	//This point shouldn't be reached if the string is correctly made
	return -1;
}

void InternalWindow::UpdateContentDimensions(){
	mContentDimensions.x = mDimensions.x + mInnerBorder;
	mContentDimensions.y = mDimensions.y + mInnerBorder;
	mContentDimensions.w = mDimensions.w-mInnerBorder*2;
	mContentDimensions.h = mDimensions.h-mInnerBorder*2;

	for(auto &option : mOptions){
		option->SetWidth(mContentDimensions.w);
	};
}

bool InternalWindow::PointInsideInnerBorder(SDL_Point point, Border *border){
	if(border == nullptr){
		return SDL_PointInRect(&point, &mDimensions) && !SDL_PointInRect(&point, &mContentDimensions);
	}
	
	//Checks if the point is inside the x and y values of the window
	bool insideX = (point.x > mDimensions.x && point.x < mDimensions.x+mDimensions.w), insideY = (point.y > mDimensions.y && point.y < mDimensions.y+mDimensions.h);

	if(insideX){
		if(point.y >= mDimensions.y && point.y < mContentDimensions.y){
			*border = Border::TOP;
			return true;
		} 
		else if(point.y <= mDimensions.y + mDimensions.h && point.y > mContentDimensions.y + mContentDimensions.h){
			*border = Border::BOTTOM;
			return true;
		}
	}
	if(insideY){
		if(point.x >= mDimensions.x && point.x < mContentDimensions.x){
			*border = Border::LEFT;
			return true;
		}
		else if(point.x <= mDimensions.x + mDimensions.w && point.x > mContentDimensions.x + mContentDimensions.w){
			*border = Border::RIGHT;
			return true;
		}
	}
	
	*border = Border::NONE;
	return false;
}

//MAIN OPTION METHODS:

MainOption::MainOption(SDL_Rect nDimensions, const char* text) : mDimensions(nDimensions), mText(text, AppManager::GetAppFont()){
	mText.SetHeight(mDimensions.h);
	mText.SetX(mDimensions.x+(mDimensions.w-mText.GetWidth())/2);
}

bool MainOption::HandleEvent(SDL_Event *pEvent){
	if(pEvent->type == SDL_MOUSEBUTTONDOWN){
		SDL_Point mousePos = {pEvent->button.x, pEvent->button.y};
		if(SDL_PointInRect(&mousePos, &mDimensions)){
			return true;
		}
	}
	
	return false;
}

void MainOption::Draw(SDL_Renderer *pRenderer){
	FillRect(pRenderer, mDimensions, 200, 200, 200);
	mText.Draw(pRenderer);
	DrawRect(pRenderer, mDimensions, 50, 50, 50);
}

//MAIN BAR METHODS:

MainBar::MainBar(SDL_Rect nDimensions) : mDimensions(nDimensions){
	SDL_Rect selectionDimensions = {mDimensions.x, mDimensions.y, mDimensions.h*6, mDimensions.h}; 
	mMainOptions.push_back(MainOption(selectionDimensions, "SAVE"));

	selectionDimensions.x += selectionDimensions.w;
	mMainOptions.push_back(MainOption(selectionDimensions, "CLEAR"));

	selectionDimensions.x += selectionDimensions.w;
	mMainOptions.push_back(MainOption(selectionDimensions, "NEW CANVAS"));

	selectionDimensions.x += selectionDimensions.w;
	mMainOptions.push_back(MainOption(selectionDimensions, "PREFERENCES"));
}
 
bool MainBar::HandleEvent(SDL_Event *pEvent){
	if(pEvent->type == SDL_MOUSEBUTTONDOWN){
		SDL_Point mousePos = {pEvent->button.x, pEvent->button.y};
		if(SDL_PointInRect(&mousePos, &mDimensions)){
			bool handledInput;

			for(int i = 0; i < mMainOptions.size(); ++i){
				handledInput = mMainOptions[i].HandleEvent(pEvent);
				
				if(handledInput){
					mCurrentClickedIndex = i;
					return true;
				}
			}
			mCurrentClickedIndex = -1;

			return true;
		}
	} else {
		mCurrentClickedIndex = -1;
	}
	
	return false;
}

void MainBar::Draw(SDL_Renderer *pRenderer){
	FillRect(pRenderer, mDimensions, 150, 150, 150);
	DrawRect(pRenderer, mDimensions, 0, 0, 0);

	for(auto &selection : mMainOptions){
		selection.Draw(pRenderer);
	}
}

std::shared_ptr<OptionInfo> MainBar::GetData(){
	if(mCurrentClickedIndex == -1) return std::shared_ptr<OptionInfo>(new OptionInfo());
	else switch(mCurrentClickedIndex){
		case static_cast<int>(MainOptionIDs::SAVE):
		case static_cast<int>(MainOptionIDs::CLEAR):
		case static_cast<int>(MainOptionIDs::NEW_CANVAS):
		case static_cast<int>(MainOptionIDs::PREFERENCES):
			return std::shared_ptr<OptionInfo>(new OptionInfo(static_cast<OptionInfo::OptionIDs>(mCurrentClickedIndex), true));
		default:
			ErrorPrint("Current clicked index could not be converted into a main option: "+std::to_string(mCurrentClickedIndex));
			return std::shared_ptr<OptionInfo>(new OptionInfo());
	}
	
}



int AppManager::mWidth;
int AppManager::mHeight;
int AppManager::mMaximumWidth = 1000;
int AppManager::mMaximumHeight = 1000;
std::shared_ptr<TTF_Font> AppManager::mpFont;
int AppManager::mMainBarHeight;
std::vector<std::unique_ptr<InternalWindow>> AppManager::mInternalWindows;
//APP WINDOW METHODS:

AppManager::AppManager(int nWidth, int nHeight, Uint32 nFlags, const char* pWindowsName){
    mpWindow.reset(SDL_CreateWindow(pWindowsName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, nWidth, nHeight, nFlags));

    //Setting width and height, checking if it is a fullscreen window
    if(nFlags & (SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_MAXIMIZED)){
        SDL_GetWindowSize(mpWindow.get(), &mWidth, &mHeight);
    } else {
        mWidth = nWidth;
        mHeight = nHeight;
    }

    mpRenderer.reset(SDL_CreateRenderer(mpWindow.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE ));

	mpCanvas.reset(new Canvas(mpRenderer.get(), 1, 1));
	mpCanvas->viewport = {0, mMainBarHeight, mWidth, mHeight-mMainBarHeight};
	mpCanvas->SetSavePath("NewImage.png");
	NewCanvas(100, 100);
	InitializeFromFile();

	mMainBarHeight = 20;
	mpMainBar.reset(new MainBar({0, 0, mWidth, mMainBarHeight}));
	
	InitializeWindow("PencilWindow");
	InitializeWindow("LayerWindow");
}

void AppManager::AddImage(const std::string &imagePath){

	SDL_Point imageSize;
	std::string imageFormat = imagePath.substr(imagePath.find_last_of('.'));
	if(imageFormat == ".png"){
		imageSize = GetSizeOfPNG(imagePath.c_str());
		if(imageSize.x  <= 0 || imageSize.x > mMaximumWidth || imageSize.y <= 0 || imageSize.y > mMaximumHeight){
			ErrorPrint(std::to_string(imageSize.x) + "x" + std::to_string(imageSize.y) + " are not valid dimensions (check png corruption or app's maximum values)");
			return;
		}
	} else if (imageFormat == ".bmp"){
		imageSize = GetSizeOfBMP(imagePath.c_str());
		if(imageSize.x  <= 0 || imageSize.x > mMaximumWidth || imageSize.y <= 0 || imageSize.y > mMaximumHeight){
			ErrorPrint(std::to_string(imageSize.x) + "x" + std::to_string(imageSize.y) + " are not valid dimensions (check bmp corruption or app's maximum values)");
			return;
		}
	} else {
		ErrorPrint("The image " + imagePath + " has the invalid format" + imageFormat);
		return;
	}

	mpCanvas->OpenFile(mpRenderer.get(), imagePath.c_str());
	mpCanvas->CenterInViewport();
	mpCanvas->SetResolution(std::min(mWidth/(float)mpCanvas->GetImageSize().x, (mHeight-mMainBarHeight)/(float)mpCanvas->GetImageSize().y)*0.9f);
}

void AppManager::NewCanvas(int width, int height){
	if(width <= 0 || width > mMaximumWidth){
		ErrorPrint("Tried to set witdh to "+std::to_string(width)+" when it can only take values from 1 to "+std::to_string(mMaximumWidth)+". Setting it to 100");
		width = 100;
	}
	if(height <= 0 || height > mMaximumHeight){
		ErrorPrint("Tried to set height to "+std::to_string(height)+" when it can only take values from 1 to "+std::to_string(mMaximumHeight)+". Setting it to 100");
		height = 100;
	}
	
	mpCanvas->Resize(mpRenderer.get(), width, height);
	mpCanvas->CenterInViewport();
	mpCanvas->SetResolution(std::min(mWidth/(float)mpCanvas->GetImageSize().x, (mHeight-mMainBarHeight)/(float)mpCanvas->GetImageSize().y)*0.9f);
}

Canvas *AppManager::GetCanvas(){
	return mpCanvas.get();
}

Uint32 AppManager::GetWindowID(){
	return SDL_GetWindowID(mpWindow.get());
}

void AppManager::HandleEvent(SDL_Event *event){
	bool hasBeenHandled = false;

	if(event->type == SDL_DROPFILE){
		const char* sourceDir = event->drop.file;
		AddImage(sourceDir);
		return; //No other 
    }

	//Just makes sure that the text input stops when any non text field is clicked
	TextInputManager::HandleEvent(event);

	hasBeenHandled = mpMainBar->HandleEvent(event);

	ProcessMainBarData();

	//We handle the events counter clockwise in order that those windows drawn last (and therefore may overlap others) are the ones who first get the events
	for(auto it = mInternalWindows.rbegin(); it != mInternalWindows.rend(); it++){
		hasBeenHandled = (*it)->HandleEvent(event);

		if(hasBeenHandled){
			SDL_Keymod keymod = SDL_GetModState();
			if(keymod & KMOD_CTRL){
				auto base = (it+1).base();
				std::rotate(base, base+1, mInternalWindows.end());
			} else if(keymod & KMOD_ALT){
				(*it)->Minimize();
			}
			break;
		}
	}

	ProcessWindowsData();

	if(!hasBeenHandled) mpCanvas->HandleEvent(event);
}

void AppManager::Update(float deltaTime){
	static float windowTimer = 0.0f;
	windowTimer += deltaTime*0.1f;

	for(auto &window : mInternalWindows) window->Update(deltaTime);

	mpCanvas->Update(deltaTime);

	int state = (int)windowTimer;
	switch(state){
		case 0: 
			mpCanvas->backgroundColor.r = 255;
			mpCanvas->backgroundColor.g = (Uint8)(255*windowTimer);
			mpCanvas->backgroundColor.b = 0;
			break;
		case 1: 
			mpCanvas->backgroundColor.r = (Uint8)(255*(2.0f-windowTimer));
			mpCanvas->backgroundColor.g = 255;
			mpCanvas->backgroundColor.b = 0;
			break;
		case 2: 
			mpCanvas->backgroundColor.r = 0;
			mpCanvas->backgroundColor.g = 255;
			mpCanvas->backgroundColor.b = (Uint8)(255*(windowTimer-2.0f));
			break;
		case 3: 
			mpCanvas->backgroundColor.r = 0;
			mpCanvas->backgroundColor.g = (Uint8)(255*(4.0f-windowTimer));
			mpCanvas->backgroundColor.b = 255;
			break;
		case 4: 
			mpCanvas->backgroundColor.r = (Uint8)(255*(windowTimer-4.0f));
			mpCanvas->backgroundColor.g = 0;
			mpCanvas->backgroundColor.b = 255;
			break;
		case 5: 
			mpCanvas->backgroundColor.r = 255;
			mpCanvas->backgroundColor.g = 0;
			mpCanvas->backgroundColor.b = (Uint8)(255*(6.0f-windowTimer));
			break;
		default:
			windowTimer = 0;
	}
}

void AppManager::Draw(){
	SDL_SetRenderDrawColor(mpRenderer.get(), 255, 255, 255, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(mpRenderer.get());
	
	SDL_SetRenderDrawBlendMode(mpRenderer.get(), SDL_BlendMode::SDL_BLENDMODE_BLEND);

	mpCanvas->DrawIntoRenderer(mpRenderer.get());
	
	for(auto &window : mInternalWindows) window->Draw(mpRenderer.get());

	mpMainBar->Draw(mpRenderer.get());

	SDL_RenderPresent(mpRenderer.get());
}

void AppManager::GetWindowSize(int &width, int &height){
	width = mWidth;
	height = mHeight;
}

int AppManager::GetMinimumWindowY(){
	return mMainBarHeight;
}

std::shared_ptr<TTF_Font> AppManager::GetAppFont(){
	return mpFont;
}

void AppManager::InitializeWindow(const std::string &windowName){
	for(const auto& window : mInternalWindows){
		if(window->GetName() == windowName){
			DebugPrint("Window "+ windowName +" is already open");
			return;
		}
	}
	
	std::ifstream windowFile("InternalData/"+windowName+".txt");

	if(windowFile.is_open()){
		std::string line, file = "";
		int options = 0;

		while(!windowFile.eof()){
			std::getline(windowFile, line, '\n');
			
			//We check wether the line is empty so no empty lines are used as options (makes sense)
			if(!line.empty() && line[0] != '#'){
				file += line+"\n"; //We add the '\n' as std::getline doesn't add it
				options++; //Each line is a new option
			}
		}

		mInternalWindows.push_back(std::make_unique<InternalWindow>(SDL_Point{200, 28}, InternalWindow::InitializationData{windowName, options, file}, mpRenderer.get()));

		windowFile.close();
	} else {
		ErrorPrint("Could not open the file InternalData/"+windowName+".txt");
		return;
	}
}

void AppManager::InitializeFromFile(){
	//Current file holding the basic app data
	std::ifstream initializationFile("InternalData/InitializationData.txt");
	
	if(initializationFile.is_open()){
		std::string line;

		while(!initializationFile.eof()){
			std::getline(initializationFile, line, '\n');
			
			if(!line.empty()){
				switch(line[0]){
					//This character indicates that the rest of the line is a comment, so we just break
					case '#': break;

					//This character indicates that the font of the app is going to be specified
					case 'F':
						if(line[1] != ':'){
							ErrorPrint("Could not read app's font, as the ':' after the 'F' is missing");
						} else {
							mpFont.reset(LoadFont("Fonts/"+line.substr(2), 72), PointerDeleter{});
						}
						break;
					
					//This character indicates the maximum width that the created canvas can have (to prevent crashes because of unsuficient memory)
					case 'W':
						if(line[1] != ':'){
							ErrorPrint("Could not read app's maximum width, as the ':' after the 'W' is missing");
						} else {
							mMaximumWidth = stoi(line.substr(2));
						}
						break;

					//This character indicates the maximum hieght that the created canvas can have (to prevent crashes because of unsuficient memory)
					case 'H':
						if(line[1] != ':'){
							ErrorPrint("Could not read app's maximum height, as the ':' after the 'H' is missing");
						} else {
							mMaximumHeight = stoi(line.substr(2));
						}
						break;

					//This character indicates the image that the program will open upon start
					case 'I':
						if(line[1] != ':'){
							ErrorPrint("Could not read app's starting image, as the ':' after the 'I' is missing");
						} else {
							AddImage(line.substr(2).c_str());
						}
						break;
				}
			}
		}

	} else {
		ErrorPrint("Could not open the file InternalData/InitializationData.txt");
	}

	initializationFile.close();
}

void AppManager::ProcessMainBarData(){
	auto data = mpMainBar->GetData();

	//If no data, there's nothing to handle
	if(data->IsInvalid()) return;

	MainBar::MainOptionIDs mainOptionID = static_cast<MainBar::MainOptionIDs>(data->optionID);

	switch(mainOptionID){
		case MainBar::MainOptionIDs::SAVE:
			mpCanvas->Save();
			break;
		case MainBar::MainOptionIDs::CLEAR:
			//Currently the current layer is cleared with no color
			mpCanvas->Clear(SDL_Color{255, 255, 255, SDL_ALPHA_TRANSPARENT});
			break;
		case MainBar::MainOptionIDs::NEW_CANVAS:
			InitializeWindow("NewCanvasWindow");
			break;
		case MainBar::MainOptionIDs::PREFERENCES:
			InitializeWindow("PreferencesWindow");
			break;
		default:
			ErrorPrint("Unable to tell the main option id: "+std::to_string(static_cast<int>(mainOptionID)));
			break;
	}
}

void AppManager::ProcessWindowsData(){
	auto safeDataApply = []<typename T>(const OptionInfo* option, const std::function<void(T)> &function){
		auto data = option->GetData<T>();
		if(data.has_value()){
			function(data.value());
		} else {
			ErrorPrint("option did not have a valid value");
		}
	};

	for(int i = 0; i < mInternalWindows.size(); i++){
		auto windowData = mInternalWindows[i]->GetData();

		//TODO: look into ways to avoid code repetition
		for(auto &option : windowData){
			switch(option->optionID){
				case OptionInfo::OptionIDs::DRAWING_COLOR:{
					std::function<void(SDL_Color)>  fn = std::bind(&Canvas::SetColor, mpCanvas.get(), std::placeholders::_1);
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::HARD_OR_SOFT:{
					auto mLambda = [this](bool toHard){
						mpCanvas->pencil.SetPencilType(toHard ? Pencil::PencilType::HARD : Pencil::PencilType::SOFT);
					};
					std::function<void(bool)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::PENCIL_RADIUS:{
					auto mLambda = [this](float radius){
						mpCanvas->pencil.SetRadius((int)radius);
					};
					std::function<void(float)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::PENCIL_HARDNESS:{
					std::function<void(float)> fn = std::bind(&Pencil::SetHardness, &(mpCanvas->pencil), std::placeholders::_1);
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::SOFT_ALPHA_CALCULATION:{
					auto mLambda = [this](int alphaMode){
						mpCanvas->pencil.SetAlphaCalculation(static_cast<Pencil::AlphaCalculation>(alphaMode));
					};
					std::function<void(int)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::CHOOSE_TOOL:{
					auto mLambda = [this](int chosenTool){
						mpCanvas->usedTool = static_cast<Canvas::Tool>(chosenTool);
					};
					std::function<void(int)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::ADD_LAYER:{
					auto mLambda = [this](bool addLayer){
						if(addLayer){
							mpCanvas->GetImage()->AddLayer(mpRenderer.get());
							
							//We update select layer slider max
							Option *layerSelector = FindOption(OptionInfo::OptionIDs::SELECT_LAYER);
							if(layerSelector) layerSelector->FetchInfo("SliderMax/"+std::to_string(mpCanvas->GetImage()->GetTotalLayers()-1)+"_InitialValue/"+std::to_string(mpCanvas->GetImage()->GetLayer())+"_");
						} else {
							ErrorPrint("ADD_LAYER data was false! (Should never happen)");
						}
					};
					std::function<void(bool)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::REMOVE_CURRENT_LAYER:{
					auto mLambda = [this](bool deleteLayer){
						if(deleteLayer){
							mpCanvas->GetImage()->DeleteCurrentLayer();
							
							//We update select layer slider max
							Option *layerSelector = FindOption(OptionInfo::OptionIDs::SELECT_LAYER);
							if(layerSelector) layerSelector->FetchInfo("SliderMax/"+std::to_string(mpCanvas->GetImage()->GetTotalLayers()-1)+"_InitialValue/"+std::to_string(mpCanvas->GetImage()->GetLayer())+"_");
						} else {
							ErrorPrint("REMOVE_CURRENT_LAYER data was false! (Should never happen)");
						}
					};
					std::function<void(bool)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::SELECT_LAYER:{
					auto mLambda = [this](float layer){
						mpCanvas->GetImage()->SetLayer((int)layer);
					};
					std::function<void(float)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::NEW_CANVAS_WIDTH:{
					//No need to use the specific data
					mInternalWindows[i]->AddTemporalData(option.get());
					break;
				}
				case OptionInfo::OptionIDs::NEW_CANVAS_HEIGHT:{
					//No need to use the specific data
					mInternalWindows[i]->AddTemporalData(option.get());
					break;
				}
				case OptionInfo::OptionIDs::NEW_CANVAS_CREATE:{
					auto mLambda = [this, &i](bool newCanvas){
						if(!newCanvas){
							ErrorPrint("NEW_CANVAS_CREATE data was false! (Should never happen)");
						} else {
							auto &temporalData = mInternalWindows[i]->GetTemporalData();
							
							//We search for the width and height of the new canvas that the user may have set
							auto widthOption = std::find_if(temporalData.begin(), temporalData.end(), [](decltype(*temporalData.begin()) option){return option->optionID == OptionInfo::OptionIDs::NEW_CANVAS_WIDTH;});
							auto heightOption = std::find_if(temporalData.begin(), temporalData.end(), [](decltype(*temporalData.begin()) option){return option->optionID == OptionInfo::OptionIDs::NEW_CANVAS_HEIGHT;});
							
							//TODO: If no width/height was specified we currently use a default value (100), may be changed into an error in the future
							int width = (widthOption == temporalData.end() || !(*widthOption)->GetData<int>().has_value()) ? 100 : (*widthOption)->GetData<int>().value();
							int height = (heightOption == temporalData.end() || !(*heightOption)->GetData<int>().has_value()) ? 100 : (*heightOption)->GetData<int>().value();

							NewCanvas(width, height);
							temporalData.clear();

							//Finally we delete this window as it's only purporse is to create a new canvas
							mInternalWindows.erase(mInternalWindows.begin()+i);
							i--;
						}
					};
					std::function<void(bool)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::SAVING_NAME:{
					auto mLambda = [this](std::string text){
						if(text.empty()){
							mpCanvas->SetSavePath("NewImage.png");
						} else {
							mpCanvas->SetSavePath((text + ".png").c_str());
						}
					};
					std::function<void(std::string)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::PENCIL_DISPLAY_MAIN_COLOR:{
					auto mLambda = [this](SDL_Color pencilDisplay){
						mpCanvas->pencilDisplayMainColor = pencilDisplay;
					};
					std::function<void(SDL_Color)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::PENCIL_DISPLAY_ALTERNATE_COLOR:{
					auto mLambda = [this](SDL_Color pencilDisplay){
						mpCanvas->pencilDisplayAlternateColor = pencilDisplay;
					};
					std::function<void(SDL_Color)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::CANVAS_MOVEMENT_SPEED:{
					auto mLambda = [this](int speed){
						mpCanvas->defaultMovementSpeed = speed;
					};
					std::function<void(int)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::CANVAS_MOVEMENT_FAST_SPEED:{
					auto mLambda = [this](int speed){
						mpCanvas->fastMovementSpeed = speed;
					};
					std::function<void(int)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				default:
					ErrorPrint("Unable to tell the option id: "+std::to_string(static_cast<int>(option->optionID)));
					break;
			}
		}
	}
}

Option *AppManager::FindOption(OptionInfo::OptionIDs optionID){
	for(const auto &window : mInternalWindows){
		for(const auto &option : window->mOptions){
			if(option->GetOptionID() == optionID){
				return option.get();
			}
		}
	}

	DebugPrint("Could not find an Option with the specified optionID: " + std::to_string(static_cast<int>(optionID)));
	return nullptr;
}