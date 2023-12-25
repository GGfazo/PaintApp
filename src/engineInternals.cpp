#include "engineInternals.hpp"
#include "renderLib.hpp"
#include "logger.hpp"
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <charconv>
#include <ranges>

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

std::vector<Option*> InternalWindow::GetOptionsWithTag(Option::Tag identifier){
	std::vector<Option*> result;

	for(const auto& option : mOptions){
		if(option->HasTag(identifier)){
			result.push_back(option.get());
		}
	}

	return result;
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

	int totalHeight = 0;
	for(auto &option : mOptions){
		option->SetY(totalHeight);

		if(option->mActive) totalHeight += option->mDimensions.h;

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

    mpRenderer.reset(SDL_CreateRenderer(mpWindow.get(), -1, SDL_RENDERER_ACCELERATED));// | SDL_RENDERER_PRESENTVSYNC ));

	InitializeFromFile();
	Option::SetOptionsFont(mpFont);

	mMainBarHeight = 20;
	mpMainBar.reset(new MainBar({0, 0, mWidth, mMainBarHeight}));
	
	mpCanvas.reset(new Canvas(mpRenderer.get(), 1, 1));
	mpCanvas->viewport = {0, mMainBarHeight, mWidth, mHeight-mMainBarHeight};
	mpCanvas->SetSavePath("NewImage.png");
	NewCanvas(100, 100);

	InitializeWindow("ToolWindow");
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
		return;
    }

	//Just makes sure that the text input stops when any non text field is clicked
	TextInputManager::HandleEvent(event);

	hasBeenHandled = mpMainBar->HandleEvent(event);

	//We handle the events counter clockwise so that those windows drawn last (and therefore may be on top of others) are the ones who first get the events
	for(auto it = mInternalWindows.rbegin(); it != mInternalWindows.rend(); it++){
		hasBeenHandled = (*it)->HandleEvent(event);

		if(hasBeenHandled){
			SDL_Keymod keymod = SDL_GetModState();
			if(keymod & KMOD_CTRL){
				auto base = (it+1).base();
				std::rotate(base, base+1, mInternalWindows.end()); //We move the window to the end, so that it's draws on the front
			} else if(keymod & KMOD_ALT){
				(*it)->Minimize();
			}
			break;
		}
	}

	if(!hasBeenHandled && !SDL_IsTextInputActive()) hasBeenHandled = HandleHotkeys(event);

	if(!hasBeenHandled) mpCanvas->HandleEvent(event);
}

void AppManager::Update(float deltaTime){
	static float windowTimer = 0.0f;
	windowTimer += deltaTime*0.1f;

	static float frameTime = 0.0f;
	static int frames = 0;
	frameTime += deltaTime;
	frames++;
	if(frameTime >= 1.0f){
		DebugPrint("FPS: "+std::to_string(frames/frameTime));
		frameTime = 0;
		frames = 0;
	}

	for(auto &window : mInternalWindows) window->Update(deltaTime);

	ProcessMainBarData();
	ProcessWindowsData();

	mpCanvas->Update(deltaTime);
	
	ProcessCommandData(mpCanvas->GiveCommands());

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

Option *AppManager::FindOption(OptionInfo::OptionIDs optionID, InternalWindow *&pWindow){
	for(const auto &window : mInternalWindows){
		for(const auto &option : window->mOptions){
			if(option->GetOptionID() == optionID){
				pWindow = window.get();
				return option.get();
			}
		}
	}

	DebugPrint("Could not find an Option with the specified optionID: " + std::to_string(static_cast<int>(optionID)));
	return nullptr;
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

bool AppManager::HandleHotkeys(SDL_Event *pEvent){
	if(pEvent->type == SDL_KEYDOWN){
		switch(pEvent->key.keysym.sym){
			case SDLK_r: FindOption(OptionInfo::OptionIDs::CHOOSE_TOOL)->FetchInfo("InitialValue/0_"); return true;
			case SDLK_f: FindOption(OptionInfo::OptionIDs::CHOOSE_TOOL)->FetchInfo("InitialValue/1_"); return true;
			case SDLK_v: FindOption(OptionInfo::OptionIDs::CHOOSE_TOOL)->FetchInfo("InitialValue/2_"); return true;
			case SDLK_t: FindOption(OptionInfo::OptionIDs::SELECT_LAYER)->FetchInfo("InitialValue/"+std::to_string(mpCanvas->GetImage()->GetLayer()+1)+"_"); return true;
			case SDLK_g: FindOption(OptionInfo::OptionIDs::SELECT_LAYER)->FetchInfo("InitialValue/"+std::to_string(mpCanvas->GetImage()->GetLayer()-1)+"_"); return true;
			case SDLK_SPACE: FindOption(OptionInfo::OptionIDs::ADD_LAYER)->FetchInfo("InitialValue/T_"); return true;
		}
	}

	return false;
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
						Pencil *canvasPencil = mpCanvas->GetTool<Pencil>();
						if(canvasPencil) canvasPencil->SetPencilType(toHard ? Pencil::PencilType::HARD : Pencil::PencilType::SOFT);
					};
					std::function<void(bool)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::TOOL_RADIUS:{
					auto mLambda = [this](float radius){
						mpCanvas->SetRadius((int)radius);
					};
					std::function<void(float)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::PENCIL_HARDNESS:{
					Pencil *canvasPencil = mpCanvas->GetTool<Pencil>();
					if(canvasPencil){
						std::function<void(float)> fn = std::bind(&Pencil::SetHardness, canvasPencil, std::placeholders::_1);
						safeDataApply(option.get(), fn);
					}
					break;
				}
				case OptionInfo::OptionIDs::SOFT_ALPHA_CALCULATION:{
					auto mLambda = [this](int alphaMode){
						Pencil *canvasPencil = mpCanvas->GetTool<Pencil>();
						if(canvasPencil) canvasPencil->SetAlphaCalculation(static_cast<Pencil::AlphaCalculation>(alphaMode));
					};
					std::function<void(int)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::CHOOSE_TOOL:{
					auto mLambda = [this](int chosenTool){
						mpCanvas->SetTool(static_cast<Canvas::Tool>(chosenTool));
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

							//Finally, as a new canvas was created, we update select layer slider max
							Option *layerSelector = FindOption(OptionInfo::OptionIDs::SELECT_LAYER);
							if(layerSelector) layerSelector->FetchInfo("SliderMax/"+std::to_string(mpCanvas->GetImage()->GetTotalLayers()-1)+"_InitialValue/"+std::to_string(mpCanvas->GetImage()->GetLayer())+"_");
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
						mpCanvas->toolPreviewMainColor = pencilDisplay;
					};
					std::function<void(SDL_Color)> fn = mLambda;
					safeDataApply(option.get(), fn);
					break;
				}
				case OptionInfo::OptionIDs::PENCIL_DISPLAY_ALTERNATE_COLOR:{
					auto mLambda = [this](SDL_Color pencilDisplay){
						mpCanvas->toolPreviewAlternateColor = pencilDisplay;
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

void AppManager::ProcessCommandData(const std::string &commands){
	if(commands.empty()) return;
	else DebugPrint(commands);

	//This might be slightly slower than a common for loop
	for (const auto& command : commands | std::views::split('\n')) {
		if(*command.begin() == '#' || command.size() == 0) continue; //'#' is used to denote comments. Also, if the command is empty, there's no point in analizing it

		//We obtain a string of the current command and the first segment of the command, located between the both indexes
		std::string stringCommand = std::string(command.begin(), command.end());
		int firstIndex = 0, lastIndex = stringCommand.find_first_of('_', firstIndex);

		if(lastIndex-firstIndex == 1 && *command.begin() == 'T'){
			//We are performing a tag command, which means the next number is the tag filtered by
			firstIndex = lastIndex + 1; lastIndex = stringCommand.find_first_of('_', firstIndex);

			//We attempt getting the tag specified in the command. Remember that the number written in the command may not match the tag itself
			unsigned long filterTag;
			try{
				std::from_chars(stringCommand.data()+firstIndex, stringCommand.data()+lastIndex, filterTag);
			}
			catch(const std::invalid_argument &e){
				ErrorPrint(stringCommand.substr(firstIndex, lastIndex-firstIndex) + " is not a valid number");
				continue;
			}
			
			std::vector<Option*> optionsWithTag;
			std::vector<InternalWindow*> windowsWithOptions; //These are needed to update their options distribution, if needed

			//We gather every option that holds the specified tag and their windows
			for(const auto& window : mInternalWindows){
				std::vector<Option*> currentOptions = window->GetOptionsWithTag(Option::PrimitiveToTag(filterTag));
				if(currentOptions.size() != 0){
					optionsWithTag.insert(optionsWithTag.end(), currentOptions.begin(), currentOptions.end());
					windowsWithOptions.push_back(window.get());
				}
			}

			//Finally we apply the commands on the affected options and update the windows in case of any resize/activation-related commands
			for(const auto& option : optionsWithTag) option->FetchInfo(stringCommand.substr(lastIndex+1));
			for(const auto& window : windowsWithOptions) window->UpdateContentDimensions();
			
		} else {
			//We are performing a common command, where the first number is the id and then a character to confirm the input method
			Option *pOption = nullptr; InternalWindow *pWindow = nullptr;

			//We attempt to get the option requested by the command
			try{
				pOption = FindOption(static_cast<OptionInfo::OptionIDs>(stoi(stringCommand.substr(firstIndex, lastIndex-firstIndex))), pWindow);
			}
			catch(const std::invalid_argument &e){
				ErrorPrint(stringCommand.substr(firstIndex, lastIndex-firstIndex) + " is not a valid number");
				continue;
			}

			//We advance to the next segment
			firstIndex = lastIndex + 1; lastIndex = stringCommand.find_first_of('_', firstIndex);

			//We check that the option exists and that the input method matches the one specified in the command
			if(pOption == nullptr || pOption->mInputMethod != pOption->CharToInputMethod(stringCommand[firstIndex])) return;

			//Finally we apply the commands on the affected option and update the window in case of any resize/activation-related commands
			pOption->FetchInfo(stringCommand.substr(lastIndex+1));
			pWindow->UpdateContentDimensions();
		}
	}
}