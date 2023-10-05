#include "engineInternals.hpp"
#include "renderLib.hpp"
#include "logger.hpp"
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>

void MainLoop(AppManager &appWindow, std::span<char*> args){
	bool keepRunning = true;
	SDL_Event ev;
	Uint64 lastUpdate = 0, currentUpdate = SDL_GetPerformanceCounter();
	float deltaTime;

	if(args.size() > 1) appWindow.AddImage(args[1]);
	appWindow.GetCanvas()->SetResolution(10);
	appWindow.GetCanvas()->SetSavePath("save.png");

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

Option::Option(InputMethod nInputMethod, SDL_Rect nDimensions, OptionInfo::OptionIDs nOptionID, std::string nInfo) : 
	mInputMethod(nInputMethod), mDimensions(nDimensions), mOptionID(nOptionID){

	//mOptionText.reset(new ConstantText(".", AppManager::GetAppFont()));

	switch(mInputMethod){
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

		case InputMethod::TICK:
			input.mpTickButton = new TickButton(SDL_Rect{MIN_SPACE, MIN_SPACE, mDimensions.h-MIN_SPACE*2, mDimensions.h-MIN_SPACE*2});
			break;
		
	}

	HandleInfo(nInfo);
}
Option::~Option(){
	switch(mInputMethod){
		case InputMethod::HEX_TEXT_FIELD:
		case InputMethod::WHOLE_TEXT_FIELD:
			delete input.mpTextField;
			break;

		case InputMethod::SLIDER:
			delete input.mpSlider;
			break;

		case InputMethod::TICK:
			delete input.mpTickButton;
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

		case InputMethod::TICK:
			eventHandled = input.mpTickButton->HandleEvent(event);
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
		
		if(SDL_Rect adjustedDimensions = {optionRect.x, optionRect.y, textRect.x + OPTION_TEXT_WIDTH, textRect.y + textRect.h}, intersectionRect;
			SDL_IntersectRect(&previousViewport, &adjustedDimensions, &intersectionRect) == SDL_TRUE){
			
			SDL_RenderSetViewport(pRenderer, &intersectionRect);
			mOptionText->Draw(pRenderer);
			SDL_RenderSetViewport(pRenderer, &optionRect);
		}
	}

	switch(mInputMethod){
		case InputMethod::HEX_TEXT_FIELD:
		case InputMethod::WHOLE_TEXT_FIELD:
			input.mpTextField->Draw(pRenderer);
			break;

		case InputMethod::SLIDER:
			input.mpSlider->Draw(pRenderer);
			break;

		case InputMethod::TICK:
			input.mpTickButton->Draw(pRenderer);
			break;
	}

	SDL_RenderSetViewport(pRenderer, &previousViewport);
}

void Option::SetWidth(int nWidth){
	mDimensions.w = nWidth;
	nWidth -= MIN_SPACE;

	switch(mInputMethod){
		case InputMethod::HEX_TEXT_FIELD:
		case InputMethod::WHOLE_TEXT_FIELD:
			input.mpTextField->dimensions.w = nWidth-input.mpTextField->dimensions.x;
			break;
			
		case InputMethod::SLIDER:
			input.mpSlider->SetWidth(std::max(nWidth-input.mpSlider->GetDimensions().x, MIN_SPACE));
			break;

		case InputMethod::TICK:
			break;
	}
}

void Option::SetHeight(int nHeight){
	mDimensions.h = nHeight;

	switch(mInputMethod){
		case InputMethod::HEX_TEXT_FIELD:
		case InputMethod::WHOLE_TEXT_FIELD:
			input.mpTextField->dimensions.h =  mDimensions.h;
			break;
			
		case InputMethod::SLIDER:{
			SDL_Rect sliderDimensions = input.mpSlider->GetDimensions();
			sliderDimensions.h = mDimensions.h;
			input.mpSlider->SetDimensions(sliderDimensions);
			break;
		}

		case InputMethod::TICK:
			input.mpTickButton->dimensions.w = mDimensions.h-MIN_SPACE*2;
			input.mpTickButton->dimensions.h = mDimensions.h-MIN_SPACE*2;
			break;
	}
}

void Option::SetOptionText(const char *pNewText){
	
	mOptionText.reset(new ConstantText(pNewText, AppManager::GetAppFont()));
	mOptionText->SetX(MIN_SPACE);
	mOptionText->SetY(MIN_SPACE);
	mOptionText->SetHeight(mDimensions.h-MIN_SPACE*2);
	
	int maxTextSpace = OPTION_TEXT_WIDTH;

	switch(mInputMethod){
		case InputMethod::HEX_TEXT_FIELD:
		case InputMethod::WHOLE_TEXT_FIELD:
			
			input.mpTextField->dimensions.x += MIN_SPACE + maxTextSpace;//mOptionText->GetWidth();
			input.mpTextField->dimensions.w -= MIN_SPACE + maxTextSpace;//mOptionText->GetWidth();
			break;
			
		case InputMethod::SLIDER:{
			
			SDL_Rect sliderDimensions = input.mpSlider->GetDimensions();
			sliderDimensions.x += MIN_SPACE + maxTextSpace;//mOptionText->GetWidth();
			sliderDimensions.w -= MIN_SPACE + maxTextSpace;//mOptionText->GetWidth();
			input.mpSlider->SetDimensions(sliderDimensions);
			
			break;
		}

		case InputMethod::TICK:
			input.mpTickButton->dimensions.x += MIN_SPACE + maxTextSpace;//mOptionText->GetWidth();
			break;
	}
}

std::shared_ptr<OptionInfo> Option::GetData(){
	if(mModified) mModified = false;
	else return std::shared_ptr<OptionInfo>(new OptionInfo());

	switch(mInputMethod){
		case InputMethod::HEX_TEXT_FIELD:
			return std::shared_ptr<OptionInfo>(new OptionInfo(mOptionID, OptionInfo::DataUsed::COLOR, input.mpTextField->GetAsColor(nullptr)));

		case InputMethod::WHOLE_TEXT_FIELD:
			return std::shared_ptr<OptionInfo>(new OptionInfo(mOptionID, OptionInfo::DataUsed::TEXT, std::string(input.mpTextField->GetText())));

		case InputMethod::SLIDER:
			return std::shared_ptr<OptionInfo>(new OptionInfo(mOptionID, OptionInfo::DataUsed::VALUE, input.mpSlider->GetValue()));

		case InputMethod::TICK:
			return std::shared_ptr<OptionInfo>(new OptionInfo(mOptionID, OptionInfo::DataUsed::TICK, input.mpTickButton->GetValue()));

		default:
			return std::shared_ptr<OptionInfo>(new OptionInfo());
	}
}

void Option::HandleInfo(std::string &info){
	//If there is no information to handle, we just return
	if(info.empty()) return;

	int previousIndex = 0, currentIndex = info.find_first_of('_', previousIndex);

	//First we check that the first data corresponds with the option's id
	if(stoul(info.substr(previousIndex, currentIndex-previousIndex)) != static_cast<unsigned int>(mOptionID)){
		ErrorPrint("String id \'"+info.substr(previousIndex, currentIndex-previousIndex)+"\' does not match with option's id "+std::to_string(static_cast<unsigned int>(mOptionID)));
		return;
	}

	//We set the index ready for the next info and declare i, the variable that will be used inside the loop 
	previousIndex = currentIndex+1;

	//We loop over all the data that until there is no more
	for(int i = info.find_first_of('_', previousIndex); i != std::string::npos; i = info.find_first_of('_', previousIndex)){
		
		Option::OptionCommands::HandleCommand(this, info.substr(previousIndex, i-previousIndex));
		
		previousIndex = i+1;
	}
}



std::unique_ptr<Option::OptionCommands::t_commands_map> Option::OptionCommands::mCommandsMap;
//OPTION COMMANDS METHODS:

void Option::OptionCommands::LoadCommands(){
	mCommandsMap.reset(new t_commands_map());
	mCommandsMap->emplace("DefaultText", OptionCommands::SetDefaultText);
	mCommandsMap->emplace("SliderMin", OptionCommands::SetMinValue);
	mCommandsMap->emplace("SliderMax", OptionCommands::SetMaxValue);
	mCommandsMap->emplace("OptionText", OptionCommands::SetOptionText);
}

void Option::OptionCommands::UnloadCommands(){
	mCommandsMap.reset();
}

void Option::OptionCommands::HandleCommand(Option *pOption, const std::string &command){
	int barIndex = command.find_first_of('/');
	if(auto it = mCommandsMap->find(command.substr(0, barIndex)); it != mCommandsMap->end()){
		it->second(pOption, command.substr(barIndex+1));
	} else {
		OptionCommands::UnusableInfo(pOption, command);
	}
}

void Option::OptionCommands::SetOptionText(Option *pOption, const std::string &nOptionText){
	pOption->SetOptionText(nOptionText.c_str());
}

void Option::OptionCommands::SetMinValue(Option *pOption, const std::string &nMin){
	if(pOption->mInputMethod != Option::InputMethod::SLIDER){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" is not a slider, inputMethod: "+std::to_string(static_cast<unsigned int>(pOption->mInputMethod)));
		return;
	}

	try{
		pOption->input.mpSlider->SetMinValue(stof(nMin));
	} catch(std::invalid_argument const &e){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" could not transform" + nMin + "into a float");
	}
}

void Option::OptionCommands::SetMaxValue(Option *pOption, const std::string &nMax){
	if(pOption->mInputMethod != Option::InputMethod::SLIDER){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" is not a slider, inputMethod: "+std::to_string(static_cast<unsigned int>(pOption->mInputMethod)));
		return;
	}

	try{
		pOption->input.mpSlider->SetMaxValue(stof(nMax));
	} catch(std::invalid_argument const &e){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" could not transform" + nMax + "into a float");
	}
}

void Option::OptionCommands::SetDefaultText(Option *pOption, const std::string &nDefaultText){
	if(pOption->mInputMethod != Option::InputMethod::WHOLE_TEXT_FIELD && pOption->mInputMethod != Option::InputMethod::HEX_TEXT_FIELD){
		ErrorPrint("id " + std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" is not a textfield, inputMethod: "+std::to_string(static_cast<unsigned int>(pOption->mInputMethod)));
		return;
	}

	pOption->input.mpTextField->SetBlankText(nDefaultText);
}

void Option::OptionCommands::UnusableInfo(Option *pOption, const std::string &nUnusableInfo){
	ErrorPrint("id "+std::to_string(static_cast<unsigned int>(pOption->mOptionID))+" found some garbage \'"+nUnusableInfo+'\'');
}

//INTERNAL WINDOW METHODS:

InternalWindow::InternalWindow(SDL_Renderer *pRenderer){
	Option::OptionCommands::LoadCommands();

	mOptions.reserve(4);
	//TODO: load dimensions and extra info from a text file with lines in the next format ('[]' are used for concepts and '()' for aclarations):
	//[ID(to verify the text to be correct)]_[extra info(literally)][\n]
	mOptions.emplace_back(new Option(Option::InputMethod::HEX_TEXT_FIELD, SDL_Rect{0, 0, mDimensions.w-mInnerBorder*2, Option::MIN_SPACE*2+mDimensions.h/10-mInnerBorder/5}, OptionInfo::OptionIDs::DRAWING_COLOR, "0_DefaultText/Hex Color_OptionText/Color_"));
	mOptions.emplace_back(new Option(Option::InputMethod::TICK, SDL_Rect{0, mOptions.back()->mDimensions.y + mOptions.back()->mDimensions.h, mDimensions.w-mInnerBorder*2, Option::MIN_SPACE*2+mDimensions.h/10-mInnerBorder/5}, OptionInfo::OptionIDs::HARD_OR_SOFT, "1_OptionText/Hard_"));
	mOptions.emplace_back(new Option(Option::InputMethod::SLIDER, SDL_Rect{0, mOptions.back()->mDimensions.y + mOptions.back()->mDimensions.h, mDimensions.w-mInnerBorder*2, Option::MIN_SPACE*2+mDimensions.h/10-mInnerBorder/5}, OptionInfo::OptionIDs::PENCIL_RADIUS, "2_SliderMin/1_SliderMax/200_OptionText/Size_"));
	mOptions.emplace_back(new Option(Option::InputMethod::SLIDER, SDL_Rect{0, mOptions.back()->mDimensions.y + mOptions.back()->mDimensions.h, mDimensions.w-mInnerBorder*2, Option::MIN_SPACE*2+mDimensions.h/10-mInnerBorder/5}, OptionInfo::OptionIDs::PENCIL_HARDNESS, "3_SliderMin/0_SliderMax/100_OptionText/Hardness_"));
	
	Option::OptionCommands::UnloadCommands();
	
	UpdateContentDimensions();
}

void InternalWindow::SetPosition(int x, int y){
	SDL_Point mainWindowSize; AppManager::GetWindowSize(mainWindowSize.x, mainWindowSize.y);
	mDimensions.x = std::clamp(x, 0, mainWindowSize.x - mDimensions.w);
	mDimensions.y = std::clamp(y, 0, mainWindowSize.y - mDimensions.h);
	UpdateContentDimensions();
}

void InternalWindow::SetSize(int w, int h){
	mDimensions.w = std::max(w, M_MIN_GAP);
	mDimensions.h = std::max(h, M_MIN_GAP);
	UpdateContentDimensions();
}

void InternalWindow::SetDimensions(const SDL_Rect &nDimensions){
	SetSize(nDimensions.w, nDimensions.h);
	SetPosition(nDimensions.x, nDimensions.y);
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
			if(Border clickedBorder; PointInsideInnerBorder({event->button.x, event->button.y}, &clickedBorder)) {
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
					mDimensions.y = std::min(event->button.y, mDraggedData.y + mDraggedData.h - M_MIN_GAP);
					mDimensions.h = mDraggedData.h + mDraggedData.y - mDimensions.y;
					UpdateContentDimensions();
					break;

				case DraggedState::RESIZE_LEFT:
					mDimensions.x = std::min(event->button.x, mDraggedData.x + mDraggedData.w - M_MIN_GAP);
					mDimensions.w = mDraggedData.w + mDraggedData.x - mDimensions.x;
					UpdateContentDimensions();
					break;

				case DraggedState::RESIZE_BOTTOM:
					mDimensions.h = std::max(event->button.y - mDimensions.y, M_MIN_GAP);
					UpdateContentDimensions();
					break;

				case DraggedState::RESIZE_RIGHT:
					mDimensions.w = std::max(event->button.x - mDimensions.x, M_MIN_GAP);
					UpdateContentDimensions();
					break;

				case DraggedState::NONE:
					wasEventHandled = false;
					break;
			}

			if(wasEventHandled) return true;
		
			//In case of mouse motion we do not care if it occurs outside of the window, as something like a slider could be dragged
			MakeEventRelativeToRect(mContentDimensions, event->motion.x, event->motion.y, originalMousePosition, eventMouseX, eventMouseY, false);
			break;
		}

		case SDL_MOUSEBUTTONUP:
			mDraggedState = DraggedState::NONE;
			
			//In case of mouse up we do not care if it occurs outside of the window, as something like a slider could be dragged
			MakeEventRelativeToRect(mContentDimensions, event->button.x, event->button.y, originalMousePosition, eventMouseX, eventMouseY, false);
			break;

		case SDL_MOUSEWHEEL:

			//The mouse wheel we want to ensure it occurs inside the window, otherwise it should be handled by another window or the canvas itself
			if(MakeEventRelativeToRect(mContentDimensions, event->wheel.mouseX, event->wheel.mouseY, originalMousePosition, eventMouseX, eventMouseY)) return false;
			break;
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
	FillRect(pRenderer, mDimensions, 100, 100, 100); // border

	FillRect(pRenderer, mContentDimensions, 200, 200, 200); 

	SDL_RenderSetViewport(pRenderer, &mContentDimensions);

	for(auto &option : mOptions){
		option->Draw(pRenderer);
	};

	SDL_RenderSetViewport(pRenderer, nullptr);
}

std::vector<std::shared_ptr<OptionInfo>> InternalWindow::GetData(){
	std::vector<std::shared_ptr<OptionInfo>> data;
	std::shared_ptr<OptionInfo> auxiliar;

	for(auto &option : mOptions){
		auxiliar = option->GetData();
		if(!auxiliar->IsInvalid()) data.push_back(auxiliar);
	};
	
	return data;
}

bool InternalWindow::MakeEventRelativeToRect(const SDL_Rect &dimensions, int &eventMouseX, int &eventMouseY, SDL_Point &originalMousePosition, bool resetIfUnable){
	//First we set the mouse original position
	originalMousePosition.x = eventMouseX;
	originalMousePosition.y = eventMouseY;

	//Then we change the coordinates and check if it falls inside the window
	eventMouseX -= dimensions.x;
	eventMouseY -= dimensions.y;

	if(eventMouseX >= 0 && eventMouseX < dimensions.w && eventMouseY >= 0 && eventMouseY < dimensions.h){
		return false;
	}

	//If it doesn't we set the values back and return true, as it falls outside the rect
	if(resetIfUnable){
		eventMouseX = originalMousePosition.x;
		eventMouseY = originalMousePosition.y;
	}
	return true;
}

bool InternalWindow::MakeEventRelativeToRect(const SDL_Rect &dimensions, int &eventMouseX, int &eventMouseY, SDL_Point &originalMousePosition, int *&pEventX, int *&pEventY, bool resetIfUnable){
	//First we set the mouse original position and the pointers 
	originalMousePosition.x = eventMouseX;
	originalMousePosition.y = eventMouseY;
	
	pEventX = &eventMouseX;
	pEventY = &eventMouseY;

	//Then we change the coordinates and check if it falls inside the window
	eventMouseX -= dimensions.x;
	eventMouseY -= dimensions.y;
	
	if(eventMouseX >= 0 && eventMouseX < dimensions.w && eventMouseY >= 0 && eventMouseY < dimensions.h){
		return false;
	}

	//If it doesn't we set the values back and return true, as it falls outside the rect
	if(resetIfUnable){
		eventMouseX = originalMousePosition.x;
		eventMouseY = originalMousePosition.y;
	}
	return true;
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



int AppManager::mWidth;
int AppManager::mHeight;
std::shared_ptr<TTF_Font> AppManager::mpFont;
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
	mpFont.reset(LoadFont("Fonts/font.ttf", 54), PointerDeleter{});

	mpWindows[0].reset(new InternalWindow(mpRenderer.get()));

	mpCanvas.reset(new Canvas(mpRenderer.get(), 100, 100));
	mpCanvas->viewport = {0, 0, mWidth, mHeight};
	mpCanvas->CenterInViewport();
	mpCanvas->pencil.SetRadius(3);

	mpColorField.reset(new TextField(mpFont, TextField::TextFormat::HEX, " "));
	mpColorField->dimensions = {0, 0, 300, 50};

	mpSizeField.reset(new TextField(mpFont, TextField::TextFormat::WHOLE_POSITIVE, " "));
	mpSizeField->dimensions = {300, 0, 300, 50};
	mpSizeField->SetText(std::to_string(mpCanvas->pencil.GetRadius()));

	mpColorPicker.reset(new PositionPickerButton(LoadTexture("Sprites/colorPicker.png", mpRenderer.get())));
	mpColorPicker->dimensions = {0, 50, 50, 50};
}

void AppManager::AddImage(const char* pImage){
	mpCanvas.reset(new Canvas(mpRenderer.get(), pImage));
	mpCanvas->viewport = {0, 0, mWidth, mHeight};
	mpCanvas->CenterInViewport();
	
	mpSizeField->SetText(std::to_string(mpCanvas->pencil.GetRadius()));
}

Canvas *AppManager::GetCanvas(){
	return mpCanvas.get();
}

Uint32 AppManager::GetWindowID(){
	return SDL_GetWindowID(mpWindow.get());
}

void AppManager::HandleEvent(SDL_Event *event){
	bool hasBeenHandled = false;

	for(auto &window : mpWindows){
		if(!hasBeenHandled) hasBeenHandled = window->HandleEvent(event);
	}
	ProcessWindowsData();

	/*hasBeenHandled = mpColorField->HandleEvent(event);
	if(hasBeenHandled) return;

	hasBeenHandled = mpSizeField->HandleEvent(event);
	if(hasBeenHandled) return;

	hasBeenHandled = mpColorPicker->HandleEvent(event);
	if(hasBeenHandled) return;*/

	if(!hasBeenHandled) mpCanvas->HandleEvent(event);
}

void AppManager::Update(float deltaTime){
	static float windowTimer = 0.0f;
	windowTimer += deltaTime*0.1f;

	for(auto &window : mpWindows) window->Update(deltaTime);

	mpCanvas->Update(deltaTime);
	
	/*mpSizeField->Update(deltaTime);

	mpColorField->Update(deltaTime);

	windowTimer += deltaTime*0.1f;

	bool isGoodData;

	if(mpColorField->HasChanged()){
		SDL_Color canvasColor = mpColorField->GetAsColor(&isGoodData);
		if(isGoodData) mpCanvas->SetColor(canvasColor);
	}

	if(mpSizeField->HasChanged()){
		int pencilSize = mpSizeField->GetAsNumber(&isGoodData);
		if(isGoodData) mpCanvas->pencil.SetRadius(pencilSize);
	}

	SDL_Point mousePosition = mpColorPicker->GetPositionPicked(&isGoodData);
	if(isGoodData){
		SDL_Point pixel = GetPointCell({mousePosition.x - mpCanvas->GetGlobalPosition().x, mousePosition.y - mpCanvas->GetGlobalPosition().y}, mpCanvas->GetResolution());
		SDL_Color pixelColor = mpCanvas->GetImage()->GetPixelColor(pixel);
		mpCanvas->SetColor(pixelColor);
		unsigned int colorHex = pixelColor.r * 0x10000 + pixelColor.g * 0x100 + pixelColor.b;

		std::stringstream s;
    	s << std::setfill('0') << std::setw(6) << std::hex << colorHex;
		mpColorField->SetText(s.str());
	}*/

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
	
	for(auto &window : mpWindows) window->Draw(mpRenderer.get());
	/*mpColorField->Draw(mpRenderer.get());
	mpSizeField->Draw(mpRenderer.get());
	mpColorPicker->Draw(mpRenderer.get());*/

	SDL_RenderPresent(mpRenderer.get());
}

void AppManager::GetWindowSize(int &width, int &height){
	width = mWidth;
	height = mHeight;
}

std::shared_ptr<TTF_Font> AppManager::GetAppFont(){
	return mpFont;
}

void AppManager::ProcessWindowsData(){
	for(auto &window : mpWindows){
		auto data = window->GetData();

		for(const auto &option : data){
			switch(option->optionID){
				case OptionInfo::OptionIDs::DRAWING_COLOR:
					mpCanvas->SetColor(option->data.color);
					break;
				case OptionInfo::OptionIDs::HARD_OR_SOFT:
					mpCanvas->pencil.pencilType = (option->data.tick ? Pencil::PencilType::HARD : Pencil::PencilType::SOFT);
					break;
				case OptionInfo::OptionIDs::PENCIL_RADIUS:
					mpCanvas->pencil.SetRadius((int)(option->data.value));
					break;
				case OptionInfo::OptionIDs::PENCIL_HARDNESS:
					mpCanvas->pencil.hardness = 0.01f*(option->data.value);
					break;
				default:
					ErrorPrint("Unable to tell the option id: "+std::to_string(static_cast<unsigned int>(option->optionID)));
					break;
			}
		}
	}
}