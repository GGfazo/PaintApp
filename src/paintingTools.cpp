#include "paintingTools.hpp"
#include "renderLib.hpp"
#include "logger.hpp"
#include <iomanip>
#include <functional>
#include <algorithm>
#include <future>
#include <sstream>

//Given a file path, returns its contents as a std::string
std::string ReadFileToString(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        ErrorPrint("Couldn't open the file "+filePath);
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return buffer.str();
}

//TOOL CIRCLE DATA FUNCTIONS:

namespace tool_circle_data{
	bool needsUpdate = true;

    int radius = 0;
    std::function<Uint8(const SDL_Point& center, const SDL_Point& position)> alphaCalculation = nullptr;

    //The alpha of 'circleColor' is not used
    SDL_Color backgroundColor = {0, 0, 0, SDL_ALPHA_TRANSPARENT}, circleColor = {255, 255, 255};
    
    float rectsResolution = 0.0f;

    //Remember that this resolution only affects the size of the rects and has no impact on the SDL_Surface
    void SetResolution(float nRectsResolution){
		rectsResolution = nRectsResolution;
		needsUpdate = true;
	}

    SDL_Surface *GetCircleSurface(){
		if(needsUpdate){
			UpdateCirclePixels();
			UpdatePreviewRects();
			needsUpdate = false;
		}

		return mpCircleSurface.get();
	}

    std::vector<SDL_Rect> &GetPreviewRects(){
		if(needsUpdate){
			UpdateCirclePixels();
			UpdatePreviewRects();
			needsUpdate = false;
		}

		return mPreviewRects;
	}
	
    void DrawPreview(SDL_Point center, SDL_Renderer *pRenderer, SDL_Color previewColor){
		if(needsUpdate){
			UpdateCirclePixels();
			UpdatePreviewRects();
			needsUpdate = false;
		}

		SDL_SetRenderDrawColor(pRenderer, previewColor.r, previewColor.g, previewColor.b, previewColor.a);
		SDL_SetRenderDrawBlendMode(pRenderer, SDL_BLENDMODE_BLEND);
		
		SDL_Rect *rects = new SDL_Rect[mPreviewRects.size()];

		for(size_t i = 0; i < mPreviewRects.size(); ++i){
			rects[i] = mPreviewRects[i];
			rects[i].x += center.x-((int)roundf(rectsResolution))/2;
			rects[i].y += center.y-((int)roundf(rectsResolution))/2;
		}
		
		SDL_RenderFillRects(pRenderer, rects, mPreviewRects.size());

		delete[] rects;
	}
 
	void UpdateCirclePixels(){
		if(radius < 0){
			ErrorPrint("radius was less than 0: "+std::to_string(radius));
			return;
		}
		if(alphaCalculation == nullptr){
			ErrorPrint("alphaCalculation was not set");
			return;
		}

		mpCircleSurface.reset(SDL_CreateRGBSurfaceWithFormat(0, 2*radius+1, 2*radius+1, 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888));
		SDL_Rect surfaceRect {0, 0, mpCircleSurface->w, mpCircleSurface->h};
		SDL_FillRect(mpCircleSurface.get(), &surfaceRect, SDL_MapRGBA(mpCircleSurface->format, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a));
		//(radius, radius) is the middle pixel for the surface
		const SDL_Point center = {radius, radius};

		//Based on "Midpoint circle algorithm - Jesko's Method", in such a way so that there are no repeating points
		int x = 0, y = radius;
		int t1 = radius/16, t2;

		auto circleCicle = [&](){
			x++;
			t1 += x;
			t2 = t1 - y;
			if(t2 >= 0){
				int xMinus = x-1;
				if(xMinus!=y){
					FillHorizontalLine(center.y+y, center.x-xMinus, center.x+xMinus, center);
					FillHorizontalLine(center.y-y, center.x-xMinus, center.x+xMinus, center);
				}
				t1 = t2;
				y--;
			} 
		};

		FillHorizontalLine(center.y, center.x-y, center.x+y, center);
		circleCicle();
		
		while(y >= x){
			FillHorizontalLine(center.y+x, center.x-y, center.x+y, center);
			FillHorizontalLine(center.y-x, center.x-y, center.x+y, center);

			circleCicle();
		}
	}

	void UpdatePreviewRects(){
		mPreviewRects.clear();

		//Currently 0,0. When displaying, the mouse coordinates will be added
		SDL_Point center = {0, 0};
		
		float tResolution = rectsResolution;
		int tRadius = radius;

		if(tResolution < 1.0f) {
			//This derives from the following:
			//FinalResolution = 1
			//OriginalResolution * (x^n) = FinalResolution
			//n = logBaseX(1/OriginalResolution)
			//FinalRadius = OriginalRadius / (x^n)
			//FinalRadius = OriginalRadius / (x^logBaseX(1/OriginalResolution))
			//FinalRadius = OriginalRadius / (1 / OriginalResolution)
			//FinalRadius = OriginalRadius * OriginalResolution
			tRadius = (int)(tRadius*tResolution);
			
			tResolution = 1.0f;
		}

		SDL_Rect auxiliar = {0, 0, 0, 0}; //Doesn't actually require initialization 
		int rectLength = 0;
		
		//Just an aproximation
		mPreviewRects.reserve(tRadius*2);

		//The method we use to round the coordinates, currently it seems like ceil produces the best looking results (aka with less gaps)
		const std::function<float(float)> ROUND = roundf;

		auto addRects = [&center, &tResolution, &ROUND](std::vector<SDL_Rect> &mPreviewRects, int rectLength, SDL_Rect &auxiliar){
			mPreviewRects.emplace_back(center.x + (int)ROUND(tResolution * auxiliar.x),                    center.y + (int)ROUND(tResolution * auxiliar.y),                    (int)ROUND(tResolution * auxiliar.w), (int)ROUND(tResolution * auxiliar.h));
			mPreviewRects.emplace_back(center.x + (int)ROUND(tResolution * auxiliar.x),                    center.y - (int)ROUND(tResolution * (auxiliar.y + auxiliar.h - 1)), (int)ROUND(tResolution * auxiliar.w), (int)ROUND(tResolution * auxiliar.h));
			mPreviewRects.emplace_back(center.x - (int)ROUND(tResolution * (auxiliar.x + auxiliar.w - 1)), center.y + (int)ROUND(tResolution * auxiliar.y),                    (int)ROUND(tResolution * auxiliar.w), (int)ROUND(tResolution * auxiliar.h));
			mPreviewRects.emplace_back(center.x - (int)ROUND(tResolution * (auxiliar.x + auxiliar.w - 1)), center.y - (int)ROUND(tResolution * (auxiliar.y + auxiliar.h - 1)), (int)ROUND(tResolution * auxiliar.w), (int)ROUND(tResolution * auxiliar.h));
			mPreviewRects.emplace_back(center.x + (int)ROUND(tResolution * auxiliar.y),                    center.y + (int)ROUND(tResolution * auxiliar.x),                    (int)ROUND(tResolution * auxiliar.h), (int)ROUND(tResolution * auxiliar.w));
			mPreviewRects.emplace_back(center.x + (int)ROUND(tResolution * auxiliar.y),                    center.y - (int)ROUND(tResolution * (auxiliar.x + auxiliar.w - 1)), (int)ROUND(tResolution * auxiliar.h), (int)ROUND(tResolution * auxiliar.w));
			mPreviewRects.emplace_back(center.x - (int)ROUND(tResolution * (auxiliar.y + auxiliar.h - 1)), center.y + (int)ROUND(tResolution * auxiliar.x),                    (int)ROUND(tResolution * auxiliar.h), (int)ROUND(tResolution * auxiliar.w));
			mPreviewRects.emplace_back(center.x - (int)ROUND(tResolution * (auxiliar.y + auxiliar.h - 1)), center.y - (int)ROUND(tResolution * (auxiliar.x + auxiliar.w - 1)), (int)ROUND(tResolution * auxiliar.h), (int)ROUND(tResolution * auxiliar.w));
		};

		//x starts at 1 since in the first 4 lines we declare the top, right, bottom and left single squares
		int x = 1, y = tRadius;
		int t1 = 1+tRadius/16, t2;
		
		mPreviewRects.emplace_back(center.x - (int)ROUND(tResolution * tRadius), center.y, (int)ROUND(tResolution), (int)ROUND(tResolution));
		mPreviewRects.emplace_back(center.x + (int)ROUND(tResolution * tRadius), center.y, (int)ROUND(tResolution), (int)ROUND(tResolution));
		mPreviewRects.emplace_back(center.x, center.y - (int)ROUND(tResolution * tRadius), (int)ROUND(tResolution), (int)ROUND(tResolution));
		mPreviewRects.emplace_back(center.x, center.y + (int)ROUND(tResolution * tRadius), (int)ROUND(tResolution), (int)ROUND(tResolution));
		
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
		if(y == x && tRadius != 1){ //I don't know why the exception for radius = 1 is needed, since it reproduces exactly as needed in any other case
			mPreviewRects.emplace_back(center.x + (int)ROUND(tResolution * x), center.y + (int)ROUND(tResolution * y), (int)ROUND(tResolution), (int)ROUND(tResolution));
			mPreviewRects.emplace_back(center.x + (int)ROUND(tResolution * x), center.y - (int)ROUND(tResolution * y), (int)ROUND(tResolution), (int)ROUND(tResolution));
			mPreviewRects.emplace_back(center.x - (int)ROUND(tResolution * x), center.y + (int)ROUND(tResolution * y), (int)ROUND(tResolution), (int)ROUND(tResolution));
			mPreviewRects.emplace_back(center.x - (int)ROUND(tResolution * x), center.y - (int)ROUND(tResolution * y), (int)ROUND(tResolution), (int)ROUND(tResolution));
		}
	}

	namespace{
        //Holds the pixels that may be modified
		std::unique_ptr<SDL_Surface, PointerDeleter> mpCircleSurface(nullptr);

        //We use rects instead of stand alone pixels, not only for efficiency but also for better displaying
		std::vector<SDL_Rect> mPreviewRects{};

        void FillHorizontalLine(int y, int minX, int maxX, const SDL_Point &circleCenter){
			for(int x = minX; x <= maxX; ++x){
				SDL_Point pos {x, y};
				SDL_Rect surf {0, 0, mpCircleSurface->w, mpCircleSurface->h};
				if(SDL_PointInRect(&pos, &surf) == SDL_TRUE){
					*UnsafeGetPixelFromSurface<Uint32>({x, y}, mpCircleSurface.get()) = SDL_MapRGBA(mpCircleSurface->format,
																									circleColor.r,
																									circleColor.g,
																									circleColor.b,
																									alphaCalculation(circleCenter, {x, y}));
				}
			}
		}
    }
};

//PENCIL METHODS:

void Pencil::Activate(){
	tool_circle_data::backgroundColor = {0, 0, 0, SDL_ALPHA_TRANSPARENT};
	tool_circle_data::circleColor = {255, 255, 255};
	tool_circle_data::alphaCalculation = GetPixelAlphaCalculation();
	tool_circle_data::needsUpdate = true;
}

void Pencil::SetHardness(float nHardness){
	mHardness = std::clamp(nHardness, 0.0f, 1.0f);
	//If the pencil is hard, then hardness has no effect on the pixels alphas
	if(mPencilType != PencilType::HARD){
		tool_circle_data::alphaCalculation = GetPixelAlphaCalculation();
		tool_circle_data::needsUpdate = true;
	}
}

float Pencil::GetHardness(){
	return mHardness;
}

void Pencil::SetAlphaCalculation(AlphaCalculation nAlphaCalculation){
	mAlphaCalculation = nAlphaCalculation;
	//If the pencil is hard, then alpha calculation mode has no effect on the pixels alphas
	if(mPencilType != PencilType::HARD){
		tool_circle_data::alphaCalculation = GetPixelAlphaCalculation();
		tool_circle_data::needsUpdate = true;
	}
}

void Pencil::SetPencilType(PencilType nPencilType){
	mPencilType = nPencilType;
	tool_circle_data::alphaCalculation = GetPixelAlphaCalculation();
	tool_circle_data::needsUpdate = true;
}

void Pencil::ApplyOn(const std::span<SDL_Point> circleCenters, SDL_Color drawColor, SDL_Surface *pSurfaceToModify, SDL_Rect *pTotalUsedArea){
	int smallestX = INT_MAX, biggestX = INT_MIN, smallestY = INT_MAX, biggestY = INT_MIN;
	bool changeWasApplied = false;

	//TODO: In the future, find a way to apply shaders (to make soft pencil faster). Currently it's preferred to use the hard pencil type for big radii

	SDL_Surface *pCircleSurface = tool_circle_data::GetCircleSurface();

	for(const auto &center : circleCenters){
		SDL_Rect drawArea = {center.x - tool_circle_data::radius, center.y - tool_circle_data::radius, 2*tool_circle_data::radius+1, 2*tool_circle_data::radius+1};
		if(mPencilType == PencilType::HARD){
			SDL_SetSurfaceColorMod(pCircleSurface, drawColor.r, drawColor.g, drawColor.b);
			SDL_SetSurfaceAlphaMod(pCircleSurface, drawColor.a);
			SDL_SetSurfaceBlendMode(pCircleSurface, SDL_BLENDMODE_BLEND);
			SDL_BlitSurface(pCircleSurface, nullptr, pSurfaceToModify, &drawArea);
			/* 
			SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(pSurfaceToModify);
			SDL_Texture* circleTexture = SDL_CreateTextureFromSurface(renderer, pCircleSurface);
			SDL_SetTextureBlendMode(circleTexture, SDL_BLENDMODE_BLEND);
			SDL_SetTextureColorMod(circleTexture, drawColor.r, drawColor.g, drawColor.b);
			SDL_SetTextureAlphaMod(circleTexture, drawColor.a);
			SDL_RenderCopy(renderer, circleTexture, nullptr, &drawArea);
			SDL_RenderPresent(renderer);
			SDL_DestroyTexture(circleTexture);
			SDL_DestroyRenderer(renderer)
			*/
		} else {

			SDL_Rect givenArea = {0, 0, pSurfaceToModify->w, pSurfaceToModify->h},  usedArea = {0,0,0,0};
			if(SDL_IntersectRect(&givenArea, &drawArea, &usedArea) == SDL_FALSE) continue;
			SDL_Point circleOffset = {usedArea.x-drawArea.x, usedArea.y-drawArea.y};
			
			float appliedAlpha = (drawColor.a/255.0f);
			FColor appliedColor = {.r = drawColor.r/255.0f, .g = drawColor.g/255.0f, .b = drawColor.b/255.0f, .a = 0};

			for(int y = 0; y < usedArea.h; ++y){
				for(int x = 0; x < usedArea.w; ++x){
					Uint32 *basePixel = UnsafeGetPixelFromSurface<Uint32>({x+usedArea.x, y+usedArea.y}, pSurfaceToModify);
					SDL_Color actualColor = {0, 0, 0, 0};
					SDL_GetRGBA(*basePixel, pSurfaceToModify->format, &actualColor.r, &actualColor.g, &actualColor.b, &actualColor.a);
					Uint32 *circlePixel = UnsafeGetPixelFromSurface<Uint32>({x+circleOffset.x, y+circleOffset.y}, pCircleSurface);
					SDL_Color circleColor; SDL_GetRGBA(*circlePixel, pCircleSurface->format, &circleColor.r, &circleColor.g, &circleColor.b, &circleColor.a); //We only care about the alpha
					
					FColor baseColor = {.r = actualColor.r/255.0f, .g = actualColor.g/255.0f, .b = actualColor.b/255.0f, .a = actualColor.a/255.0f};
					appliedColor.a = (appliedAlpha*circleColor.a)/255.0f;
					MutableTexture::ApplyColorToColor(baseColor, appliedColor);
					*basePixel = SDL_MapRGBA(pSurfaceToModify->format, round(SDL_ALPHA_OPAQUE*baseColor.r), round(SDL_ALPHA_OPAQUE*baseColor.g), round(SDL_ALPHA_OPAQUE*baseColor.b), round(SDL_ALPHA_OPAQUE*baseColor.a));
				}
			}
		}
        
		changeWasApplied = true;
		if(smallestX > center.x) smallestX = center.x;
		if(biggestX < center.x) biggestX = center.x;
		if(smallestY > center.y) smallestY = center.y;
		if(biggestY < center.y) biggestY = center.y;
	}
	
	SDL_SetSurfaceColorMod(pCircleSurface, 255, 255, 255);
	SDL_SetSurfaceAlphaMod(pCircleSurface, SDL_ALPHA_OPAQUE);

	if(pTotalUsedArea != nullptr){
		if(changeWasApplied){
			SDL_Rect actualArea = {smallestX - tool_circle_data::radius, smallestY - tool_circle_data::radius, (biggestX-smallestX) + 2*tool_circle_data::radius+1, (biggestY-smallestY) + 2*tool_circle_data::radius+1};
			SDL_Rect surfaceArea = {0, 0, pSurfaceToModify->w, pSurfaceToModify->h};
			SDL_IntersectRect(&actualArea, &surfaceArea, pTotalUsedArea);
		} else {
			*pTotalUsedArea = {0, 0, 0, 0};
		}
	}
}

void Pencil::SetResolution(float nResolution){
	tool_circle_data::rectsResolution = nResolution;
	tool_circle_data::UpdatePreviewRects();
}

void Pencil::DrawPreview(SDL_Point center, SDL_Renderer *pRenderer, SDL_Color previewColor){
	tool_circle_data::DrawPreview(center, pRenderer, previewColor);
}

std::function<Uint8(const SDL_Point& center, const SDL_Point& position)> Pencil::GetPixelAlphaCalculation(){
	if(mPencilType ==  PencilType::HARD){
		return [](const SDL_Point& center, const SDL_Point& position) -> Uint8 {return SDL_ALPHA_OPAQUE;};

	} else if(mPencilType ==  PencilType::SOFT){
		AlphaCalculation alphaCalculation = mAlphaCalculation;
		float hardness = mHardness;

		return [alphaCalculation, hardness](const SDL_Point& center, const SDL_Point& position) ->Uint8{
			float centerDistance = sqrt(pow(position.x-center.x, 2) + pow(position.y-center.y, 2));
			float maxDistance = (tool_circle_data::radius+0.5f);
			Uint8 alpha;
			
			if(centerDistance > maxDistance){
				alpha = SDL_ALPHA_TRANSPARENT;
			}
			else switch(alphaCalculation){
				case AlphaCalculation::LINEAR: 
					alpha = (Uint8)(SDL_ALPHA_OPAQUE * std::min(hardness*2.0f * (1-centerDistance/maxDistance), 1.0f));
					break;
				case AlphaCalculation::QUADRATIC:
					alpha = (Uint8)(SDL_ALPHA_OPAQUE * std::min(hardness*2.0f * (1-powf(centerDistance/maxDistance, 2)), 1.0f));
					break;
				case AlphaCalculation::EXPONENTIAL:
					alpha = (Uint8)(SDL_ALPHA_OPAQUE * std::min(hardness*2.0f * (exp(-centerDistance/maxDistance)), 1.0f));
					break;
			}

			/*
			POTENTIAL FUTURE ALPHA CALCULATIONS:
			LOGARITHMIC : alpha = (Uint8)(SDL_ALPHA_OPAQUE * std::min(hardness*2.0f * (1-std::log(1+centerDistance/maxDistance)), 1.0f));
			HYPERBOLIC : alpha = (Uint8)(SDL_ALPHA_OPAQUE * std::min(hardness*2.0f * std::tanh(centerDistance/maxDistance), 1.0f));
			INVERSE HYPERBOLIC : alpha = (Uint8)(SDL_ALPHA_OPAQUE * std::min(hardness*2.0f * std::atanh(centerDistance/maxDistance), 1.0f));
			SQUARE ROOT : alpha = (Uint8)(SDL_ALPHA_OPAQUE * std::min(hardness*2.0f * std::sqrt(centerDistance/maxDistance), 1.0f));
			*/

			return alpha;
		};
	}

	return [](const SDL_Point& center, const SDL_Point& position)->Uint8{ErrorPrint("Invoked GetPixelAlphaCalculation's invalid Lambda"); return 0;};
}

//ERASER METHODS:

void Eraser::Activate(){
	tool_circle_data::backgroundColor = {255, 255, 255, SDL_ALPHA_OPAQUE};
	tool_circle_data::circleColor = {0, 0, 0};
	tool_circle_data::alphaCalculation = [](const SDL_Point& center, const SDL_Point& position) -> Uint8 {return SDL_ALPHA_TRANSPARENT;};
	tool_circle_data::needsUpdate = true;
}

void Eraser::ApplyOn(const std::span<SDL_Point> circleCenters, SDL_Surface *pSurfaceToModify, SDL_Rect *pTotalUsedArea){
	int smallestX = INT_MAX, biggestX = INT_MIN, smallestY = INT_MAX, biggestY = INT_MIN;
	bool changeWasApplied = false;

	SDL_Surface *pCircleSurface = tool_circle_data::GetCircleSurface();
	SDL_SetSurfaceColorMod(pCircleSurface, 255, 255, 255);
	SDL_SetSurfaceAlphaMod(pCircleSurface, SDL_ALPHA_OPAQUE);
	SDL_SetSurfaceBlendMode(pCircleSurface, SDL_BLENDMODE_NONE);
		

	for(const auto &center : circleCenters){
		SDL_Rect drawArea = {center.x - tool_circle_data::radius, center.y - tool_circle_data::radius, 2*tool_circle_data::radius+1, 2*tool_circle_data::radius+1};
		SDL_Rect givenArea = {0, 0, pSurfaceToModify->w, pSurfaceToModify->h},  usedArea = {0,0,0,0};
		if(SDL_IntersectRect(&givenArea, &drawArea, &usedArea) == SDL_FALSE) continue;
		SDL_Point circleOffset = {usedArea.x-drawArea.x, usedArea.y-drawArea.y};
		
		for(int y = 0; y < usedArea.h; ++y){
			for(int x = 0; x < usedArea.w; ++x){
				Uint32 *circlePixel = UnsafeGetPixelFromSurface<Uint32>({x+circleOffset.x, y+circleOffset.y}, pCircleSurface);

				if(*circlePixel == 0){
					*UnsafeGetPixelFromSurface<Uint32>({x+usedArea.x, y+usedArea.y}, pSurfaceToModify) = 0;
				}
			}
		}
		
		changeWasApplied = true;
		if(smallestX > center.x) smallestX = center.x;
		if(biggestX < center.x) biggestX = center.x;
		if(smallestY > center.y) smallestY = center.y;
		if(biggestY < center.y) biggestY = center.y;
	}
	
	if(pTotalUsedArea != nullptr){
		if(changeWasApplied){
			SDL_Rect actualArea = {smallestX - tool_circle_data::radius, smallestY - tool_circle_data::radius, (biggestX-smallestX) + 2*tool_circle_data::radius+1, (biggestY-smallestY) + 2*tool_circle_data::radius+1};
			SDL_Rect surfaceArea = {0, 0, pSurfaceToModify->w, pSurfaceToModify->h};
			SDL_IntersectRect(&actualArea, &surfaceArea, pTotalUsedArea);
		} else {
			*pTotalUsedArea = {0, 0, 0, 0};
		}
	}

	//Assumes that the previous blend mode was blend
	SDL_SetSurfaceBlendMode(pSurfaceToModify, SDL_BLENDMODE_BLEND);
}

void Eraser::SetResolution(float nResolution){
	tool_circle_data::rectsResolution = nResolution;
	tool_circle_data::UpdatePreviewRects();
}

void Eraser::DrawPreview(SDL_Point center, SDL_Renderer *pRenderer, SDL_Color previewColor){
	tool_circle_data::DrawPreview(center, pRenderer, previewColor);
}

//COLOR PICKER METHODS:
    
void ColorPicker::Activate(){
	tool_circle_data::backgroundColor = {0, 0, 0, SDL_ALPHA_TRANSPARENT};
	tool_circle_data::circleColor = {0, 0, 0, SDL_ALPHA_TRANSPARENT};
}
    
void ColorPicker::GrabColor(Canvas *pCanvas, MutableTexture *pTexture, SDL_Point pixel){
	bool isValid = false;
	SDL_Color color = pTexture->GetPixelColor(pixel, &isValid);
	if(isValid){
		std::stringstream hexColor;
		
		//We transform the red, green and blue into hex, making sure each one takes 2 characters and, in case they weren't 2 characters wide, we fill the hex with leading '0'
		hexColor << std::hex << std::setw(2) << std::setfill('0') << ((unsigned int)color.r)
							 << std::setw(2) << std::setfill('0') << ((unsigned int)color.g)
							 << std::setw(2) << std::setfill('0') << ((unsigned int)color.b);
							 
		pCanvas->AppendCommand("0_H_InitialValue/"+hexColor.str()+"_"); //Refers to the DRAWING_COLOR hex field
	}
}
    
void ColorPicker::SetResolution(float nResolution){
	tool_circle_data::rectsResolution = nResolution;
	//tool_circle_data::UpdatePreviewRects(); -> We don't use the preview rects since we only really need to draw one
}
    
void ColorPicker::DrawPreview(SDL_Point center, SDL_Renderer *pRenderer, SDL_Color previewColor){
	SDL_SetRenderDrawColor(pRenderer, previewColor.r, previewColor.g, previewColor.b, previewColor.a);
	SDL_SetRenderDrawBlendMode(pRenderer, SDL_BLENDMODE_BLEND);
	
	SDL_Rect resultingRect ={center.x, center.y, (int)roundf(tool_circle_data::rectsResolution), (int)roundf(tool_circle_data::rectsResolution)};
	resultingRect.x -= ((int)roundf(tool_circle_data::rectsResolution))/2;
	resultingRect.y -= ((int)roundf(tool_circle_data::rectsResolution))/2;
	
	SDL_RenderFillRect(pRenderer, &resultingRect);

}

//AREA DELIMITER METHODS:
    
void AreaDelimiter::Activate(){
	tool_circle_data::backgroundColor = {0, 0, 0, SDL_ALPHA_TRANSPARENT};
	tool_circle_data::circleColor = {0, 0, 0, SDL_ALPHA_TRANSPARENT};
}

bool AreaDelimiter::HandleEvent(SDL_Event *event, SDL_FPoint mousePoint){
	if(event->type == SDL_MOUSEBUTTONDOWN){

		SDL_FPoint *closestPoint = GetPoint(mousePoint);

		if(closestPoint == nullptr){
			mPoints.push_back(mousePoint);
			mpSelectedPoint = &mPoints.back();
		} else {
			mpSelectedPoint = &(*closestPoint);
		}
		
		mPointHolded = true;
		return true;

	} else if (event->type == SDL_MOUSEMOTION){
		//We need to check that 'mpSelectedPoint' exists just in case the area was cleared while the point was still selected
		if(mPointHolded && mpSelectedPoint) *mpSelectedPoint = mousePoint;
	} else if (event->type == SDL_MOUSEBUTTONUP){
		mPointHolded = false;
	}
	return false;
}

void AreaDelimiter::EraseSelected(){
	if(mpSelectedPoint == nullptr || mPoints.empty()) return;
	int index = std::distance(mPoints.data(), mpSelectedPoint);
	mPoints.erase(mPoints.begin()+index);
	
	//'mpSelectedPoint' ends up pointing to the previous point, or the next one if at the beggining
	index--;
	if(index > 0){
		mpSelectedPoint = mPoints.data()+std::max(index, 0); 
	} else {
		mpSelectedPoint = &mPoints.back();
	}
}

void AreaDelimiter::AddBeforeSelected(){
	if(mpSelectedPoint == nullptr || mPoints.empty()) return;
	int index = std::distance(mPoints.data(), mpSelectedPoint);
	mPoints.insert(mPoints.begin()+index+1, {mpSelectedPoint->x+0.75f, mpSelectedPoint->y+0.75f});
	mpSelectedPoint = mPoints.data()+index+1; //'mpSelectedPoint' ends up pointing to the new point
}

void AreaDelimiter::Clear(){
	mpSelectedPoint = nullptr;
	mPoints.clear();
}

void AreaDelimiter::SetResolution(float nResolution){
	tool_circle_data::rectsResolution = nResolution;
}

std::vector<SDL_FPoint> AreaDelimiter::GetPointsCopy(){
	return mPoints;
}

void AreaDelimiter::DrawPreview(SDL_Point realOffset, SDL_Renderer *pRenderer, SDL_Color previewColor){
	if(mPoints.empty()) return;

	const int RECTS_SIZE = tool_circle_data::rectsResolution;
	
	if(RECTS_SIZE != 0){
		SDL_Rect *rects = new SDL_Rect[mPoints.size()];

		std::ranges::transform(mPoints, rects, [realOffset, RECTS_SIZE](const SDL_FPoint& point) {
			SDL_Rect rect;
			rect.x = tool_circle_data::rectsResolution*point.x - RECTS_SIZE/2+ realOffset.x;
			rect.y = tool_circle_data::rectsResolution*point.y - RECTS_SIZE/2 + realOffset.y;
			rect.w = RECTS_SIZE;
			rect.h = RECTS_SIZE;
			return rect;
		});

		SDL_SetRenderDrawColor(pRenderer, previewColor.r, previewColor.g, previewColor.b, previewColor.a);
		SDL_RenderFillRects(pRenderer, rects, mPoints.size());

		SDL_SetRenderDrawColor(pRenderer, 0, 0, 0, previewColor.a);
		SDL_RenderDrawRect(pRenderer, rects + std::distance(mPoints.data(), mpSelectedPoint));

		delete[] rects;
	}
}

void AreaDelimiter::DrawArea(SDL_Point realOffset, SDL_Renderer *pRenderer, SDL_Color previewColor){
	if(mPoints.size() <= 1) return;

	SDL_SetRenderDrawColor(pRenderer, previewColor.r, previewColor.g, previewColor.b, previewColor.a);

	SDL_Point *realPoints = new SDL_Point[mPoints.size()];

	std::ranges::transform(mPoints, realPoints, [realOffset](const SDL_FPoint& point) {
		SDL_Point realPoint;
		realPoint.x = tool_circle_data::rectsResolution*point.x + realOffset.x;
		realPoint.y = tool_circle_data::rectsResolution*point.y + realOffset.y;
		return realPoint;
	});

	SDL_RenderDrawLines(pRenderer, realPoints, mPoints.size());
	if(loopBack) SDL_RenderDrawLine(pRenderer, (realPoints+(mPoints.size()-1))->x, (realPoints+(mPoints.size()-1))->y, realPoints->x, realPoints->y);
}

SDL_FPoint *AreaDelimiter::GetPoint(SDL_FPoint &target){
	constexpr float MIN_NEEDED_DISTANCE = 0.5f; //Half a pixel
	float minDistance = std::hypot(MIN_NEEDED_DISTANCE, MIN_NEEDED_DISTANCE);
	SDL_FPoint *closestPoint = nullptr;

	for(auto it = mPoints.begin(); it != mPoints.end(); it++){
		float xDistance = it->x - target.x;
		float yDistance = it->y - target.y;
		if(xDistance < MIN_NEEDED_DISTANCE && yDistance < MIN_NEEDED_DISTANCE){

			float nDistance = std::hypot(xDistance, yDistance);
			if(nDistance < minDistance){
				minDistance = nDistance;
				closestPoint = &*it;
			}
		}
	}

	return closestPoint;
}

//MUTABLE TEXTURE METHODS:

MutableTexture::MutableTexture(SDL_Renderer *pRenderer, int width, int height, SDL_Color fillColor){
	mSelectedLayer = 0;
	
	mShowSurface.resize(1);
	mShowSurface[mSelectedLayer] = true;
	mpSurfaces.resize(1);
	mpSurfaces[mSelectedLayer].reset(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888));
    mpTexture.reset(SDL_CreateTexture(pRenderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, width, height));
	SDL_SetSurfaceBlendMode(mpSurfaces[mSelectedLayer].get(), SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(mpTexture.get(), SDL_BLENDMODE_BLEND);

	Clear(fillColor);
}

MutableTexture::MutableTexture(SDL_Renderer *pRenderer, const char *pImage){
	SDL_Surface *loaded = IMG_Load(pImage);
	mSelectedLayer = 0;
	
	mShowSurface.resize(1);
	mShowSurface[mSelectedLayer] = true;
	mpSurfaces.resize(1);
	mpSurfaces[mSelectedLayer].reset(SDL_ConvertSurfaceFormat(loaded, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888, 0));
    mpTexture.reset(SDL_CreateTexture(pRenderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, loaded->w, loaded->h));
	SDL_SetSurfaceBlendMode(mpSurfaces[mSelectedLayer].get(), SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(mpTexture.get(), SDL_BLENDMODE_BLEND);

    SDL_FreeSurface(loaded);
	
	UpdateWholeTexture();
}

void MutableTexture::AddFileAsLayer(SDL_Renderer *pRenderer, const char *pImage, SDL_Point imageSize){
	SDL_Point currentSize = {GetWidth(), GetHeight()};
	SDL_Point finalSize = {std::max(currentSize.x, imageSize.x), std::max(currentSize.y, imageSize.y)};

	//If the final size differs from the previous size, we need to resize all existing layers
	if(finalSize.x != currentSize.x || finalSize.y != currentSize.y) ResizeAllLayers(pRenderer, finalSize);
	AddLayer();

	SDL_Surface *loaded = IMG_Load(pImage);

	//If the final size differs from the image's size, we just blit it onto the new layer. Otherwise we just set the layer directly
	if(finalSize.x != imageSize.x || finalSize.y != imageSize.y){
		SDL_SetSurfaceBlendMode(loaded, SDL_BLENDMODE_NONE);
		SDL_BlitSurface(loaded, nullptr, mpSurfaces[mSelectedLayer].get(), nullptr);
	} else {
		mpSurfaces[mSelectedLayer].reset(loaded);
	}
	
	mShowSurface[mSelectedLayer] = true;
	
	SDL_SetSurfaceBlendMode(mpSurfaces[mSelectedLayer].get(), SDL_BLENDMODE_BLEND);
	UpdateWholeTexture();
}

SDL_Color MutableTexture::GetPixelColor(SDL_Point pixel, bool *validValue){
	
	if(IsPixelOutsideImage(pixel)){
		if(validValue) *validValue = false;
		return {0,0,0,0};
	} else {
		if(validValue) *validValue = true;
	}

	SDL_Color pixelColor = {255, 255, 255, SDL_ALPHA_TRANSPARENT};
	
	//We calculate the end displayed color
	for(size_t i = 0; i < mpSurfaces.size(); ++i){
		if(!mShowSurface[i]) continue;

		SDL_Color auxiliar;
		SDL_GetRGBA(*UnsafeGetPixel(pixel, i), mpSurfaces[i]->format, &auxiliar.r, &auxiliar.g, &auxiliar.b, &auxiliar.a);
				
		Uint8 nAlpha = 0;
		SDL_GetSurfaceAlphaMod(mpSurfaces[i].get(), &nAlpha);
		auxiliar.a = (Uint8)((nAlpha/255.0f) * auxiliar.a); //We apply the alpha mod
		
		ApplyColorToColor(pixelColor, auxiliar);
	}
	return pixelColor;
}

void MutableTexture::Clear(const SDL_Color &clearColor){
	//Currently only clears the current layer
	SDL_FillRect(mpSurfaces[mSelectedLayer].get(), nullptr, SDL_MapRGBA(mpSurfaces[mSelectedLayer]->format, clearColor.r, clearColor.g, clearColor.b, clearColor.a));

	UpdateWholeTexture();
}

void MutableTexture::ResizeAllLayers(SDL_Renderer *pRenderer, SDL_Point nSize){
	for(auto& pSurface : mpSurfaces){
		SDL_Surface *nSurface = SDL_CreateRGBSurfaceWithFormat(0, nSize.x, nSize.y, 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888);
		SDL_FillRect(nSurface, nullptr, SDL_MapRGBA(nSurface->format, 255, 255, 255, 0));
		SDL_SetSurfaceBlendMode(nSurface, SDL_BLENDMODE_BLEND);
		SDL_SetSurfaceBlendMode(pSurface.get(), SDL_BLENDMODE_NONE);
		
		SDL_BlitSurface(pSurface.get(), nullptr, nSurface, nullptr);

		pSurface.reset(nSurface);
	}

	//Finally we also need to resize the texture
    mpTexture.reset(SDL_CreateTexture(pRenderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, nSize.x, nSize.y));
	SDL_SetTextureBlendMode(mpTexture.get(), SDL_BLENDMODE_BLEND);
	UpdateWholeTexture();
}

void MutableTexture::ApplyColorToColor(SDL_Color &baseColor, const SDL_Color &appliedColor){
	FColor base {.r = baseColor.r/255.0f, .g = baseColor.g/255.0f, .b = baseColor.b/255.0f, .a = baseColor.a/255.0f};
	FColor applied{.r = appliedColor.r/255.0f, .g = appliedColor.g/255.0f, .b = appliedColor.b/255.0f, .a = appliedColor.a/255.0f};

	ApplyColorToColor(base, applied);

	baseColor.r = round(SDL_ALPHA_OPAQUE*(base.r));
	baseColor.g = round(SDL_ALPHA_OPAQUE*(base.g));
	baseColor.b = round(SDL_ALPHA_OPAQUE*(base.b));
	baseColor.a = round(SDL_ALPHA_OPAQUE*(base.a));
}

void MutableTexture::ApplyColorToColor(FColor &baseColor, const FColor &appliedColor){
	float resultA = appliedColor.a+baseColor.a*(1-appliedColor.a);
	baseColor.r = (appliedColor.r*appliedColor.a+baseColor.r*baseColor.a*(1-appliedColor.a))/resultA;
	baseColor.g = (appliedColor.g*appliedColor.a+baseColor.g*baseColor.a*(1-appliedColor.a))/resultA;
	baseColor.b = (appliedColor.b*appliedColor.a+baseColor.b*baseColor.a*(1-appliedColor.a))/resultA;
	baseColor.a = resultA; //Important that the alpha is set at the end, since it's used in the calculations

	/*
	//The following procedure comes from https://en.wikipedia.org/wiki/Alpha_compositing gamma correction:
	baseColor.r = SDL_ALPHA_OPAQUE*sqrt((apR*apR*apA+bsR*bsR*bsA*(1-apA))/(apA+bsA*(1-apA)));
	baseColor.g = SDL_ALPHA_OPAQUE*sqrt((apG*apG*apA+bsG*bsG*bsA*(1-apA))/(apA+bsA*(1-apA)));
	baseColor.b = SDL_ALPHA_OPAQUE*sqrt((apB*apB*apA+bsB*bsB*bsA*(1-apA))/(apA+bsA*(1-apA)));
	*/
}

void MutableTexture::SetPixel(SDL_Point pixel, const SDL_Color &color){
    if(IsPixelOutsideImage(pixel)) {
        ErrorPrint("pixel has an invalid index");
        return;
    }

	//It is safe to call 'SetPixelUnsafe', since we already locked the surface and checked that the localPixel does exist
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

void MutableTexture::SetPixelUnsafe(SDL_Point pixel, const SDL_Color &color){
	*UnsafeGetPixel(pixel) = SDL_MapRGBA(mpSurfaces[mSelectedLayer]->format, color.r, color.g, color.b, color.a);

	mChangedPixels.push_back(pixel);
}

void MutableTexture::SetPixelsUnsafe(std::span<SDL_Point> pixels, const SDL_Color &color){
	for(const auto &pixel : pixels){
		*UnsafeGetPixel(pixel) = SDL_MapRGBA(mpSurfaces[mSelectedLayer]->format, color.r, color.g, color.b, color.a);
	}

	mChangedPixels.insert(mChangedPixels.end(), pixels.begin(), pixels.end());
}

SDL_Surface *MutableTexture::GetSurfaceAtLayer(int layer){
	return mpSurfaces[std::clamp(layer, 0, (int)(mpSurfaces.size()-1))].get();
}

SDL_Surface *MutableTexture::GetCurrentSurface(){
	return mpSurfaces[mSelectedLayer].get();
}

void MutableTexture::UpdateTexture(){
	if(mChangedPixels.empty()) return;

	UpdateTexture(GetChangesRect());

	mChangedPixels.clear();
}

void MutableTexture::UpdateTexture(const SDL_Rect &rect){
	if(rect.w <= 0 || rect.h <= 0){
		ErrorPrint("limitating rect's width or height was less than or equal to 0 (must at least be 1)");
		return;
	}

	SDL_Surface *texturesSurface;

	SDL_LockTextureToSurface(mpTexture.get(), &rect, &texturesSurface);
	
	SDL_FillRect(texturesSurface, nullptr, SDL_MapRGBA(texturesSurface->format, 255, 255, 255, SDL_ALPHA_TRANSPARENT));
	for(size_t i = 0; i < mpSurfaces.size(); i++){
		if(mShowSurface[i]) SDL_BlitSurface(mpSurfaces[i].get(), &rect, texturesSurface, nullptr);
	}
	
	SDL_UnlockTexture(mpTexture.get());
}

void MutableTexture::AddLayer(){
	mShowSurface.emplace(mShowSurface.begin()+mSelectedLayer+1, true);
	mpSurfaces.emplace(mpSurfaces.begin()+mSelectedLayer+1, SDL_CreateRGBSurfaceWithFormat(0, GetWidth(), GetHeight(), 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888));
    mSelectedLayer++;
	
	//Currently all new layers are created with no colors
	Clear({255, 255, 255, SDL_ALPHA_TRANSPARENT});
	SDL_SetSurfaceBlendMode(mpSurfaces[mSelectedLayer].get(), SDL_BLENDMODE_BLEND);
	UpdateWholeTexture();
}

bool MutableTexture::DeleteCurrentLayer(){
	if(mpSurfaces.size() == 1){
		DebugPrint("Can't delete current layer, as it is the last one left");
		return false;
	}

	//Better safe than sorry
	mSelectedLayer = std::clamp(mSelectedLayer, 0, (int)(mpSurfaces.size()-1));

	mShowSurface.erase(mShowSurface.begin() + mSelectedLayer);
	mpSurfaces.erase(mpSurfaces.begin() + mSelectedLayer);
	
	UpdateWholeTexture();
	if(mSelectedLayer != 0) mSelectedLayer--;

	return true;
}

void MutableTexture::SetLayerVisibility(bool visible){
	mShowSurface[mSelectedLayer] = visible;

	UpdateWholeTexture();
}

bool MutableTexture::GetLayerVisibility(){
	return mShowSurface[mSelectedLayer];
}

void MutableTexture::SetLayerAlpha(Uint8 alpha){
	SDL_SetSurfaceAlphaMod(mpSurfaces[mSelectedLayer].get(), alpha);

	UpdateWholeTexture();
}

Uint8 MutableTexture::GetLayerAlpha(){
	Uint8 nAlpha = 0;
	SDL_GetSurfaceAlphaMod(mpSurfaces[mSelectedLayer].get(), &nAlpha);
	return nAlpha;
}

void MutableTexture::SetLayer(int nLayer){
	nLayer = std::clamp(nLayer, 0, (int)mpSurfaces.size()-1);
	mSelectedLayer = nLayer;
}

int MutableTexture::GetLayer(){
	return mSelectedLayer;
}

int MutableTexture::GetTotalLayers(){
	return mpSurfaces.size();
}

void MutableTexture::DrawIntoRenderer(SDL_Renderer *pRenderer, const SDL_Rect &dimensions){
	SDL_RenderCopy(pRenderer, mpTexture.get(), nullptr, &dimensions);
}

bool MutableTexture::Save(const char *pSavePath){
	std::unique_ptr<SDL_Surface, PointerDeleter> pSaveSurface(SDL_CreateRGBSurfaceWithFormat(0, GetWidth(), GetHeight(), 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888));
	SDL_FillRect(pSaveSurface.get(), nullptr, SDL_MapRGBA(pSaveSurface->format, 255, 255, 255, SDL_ALPHA_TRANSPARENT));

	for(size_t i = 0; i < mpSurfaces.size(); i++){
		//Only the surfaces that are being shown get applied
		if(mShowSurface[i]) SDL_BlitSurface(mpSurfaces[i].get(), nullptr, pSaveSurface.get(), nullptr);
	}

	if(IMG_SavePNG(pSaveSurface.get(), pSavePath)){
		ErrorPrint("Couldn't save image in file "+std::string(pSavePath));
		return true;
	}
	return false;
}

int MutableTexture::GetWidth(){
	return mpSurfaces[0]->w;
}

int MutableTexture::GetHeight(){
	return mpSurfaces[0]->h;
}

void MutableTexture::UpdateWholeTexture(){
	UpdateTexture({0, 0, GetWidth(), GetHeight()});
	
	mChangedPixels.clear();
}

SDL_Rect MutableTexture::GetChangesRect(){
	if(mChangedPixels.size() == 1){
		return {mChangedPixels[0].x, mChangedPixels[0].y, 1, 1};
	}

	SDL_Rect changesRect = {0, 0, 0, 0};
	
	SDL_EnclosePoints(mChangedPixels.data(), mChangedPixels.size(), nullptr, &changesRect);
		
	return changesRect;
}



int Canvas::maxAmountOfUndoActionsSaved = 0;
//CANVAS METHODS:

Canvas::Canvas(SDL_Renderer *pRenderer, int nWidth, int nHeight) : mpImage(new MutableTexture(pRenderer, nWidth, nHeight)), mDisplayingHolder(this){
	mActionsManager.Initialize(Canvas::maxAmountOfUndoActionsSaved);
	mDimensions = {0, 0, nWidth, nHeight};
	mDisplayingHolder.Update();
	UpdateRealPosition();
}

Canvas::Canvas(SDL_Renderer *pRenderer, const char *pLoadFile) : mpImage(new MutableTexture(pRenderer, pLoadFile)), mDisplayingHolder(this){
	mActionsManager.Initialize(Canvas::maxAmountOfUndoActionsSaved);
	mDimensions = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};
	mDisplayingHolder.Update();
	UpdateRealPosition();
}

Canvas::~Canvas(){
	if(saveOnDestroy) Save();
}

void Canvas::Resize(SDL_Renderer *pRenderer, int nWidth, int nHeight){
	mpImage.reset(new MutableTexture(pRenderer, nWidth, nHeight));
	mActionsManager.ClearData();
	mDimensions = {0, 0, nWidth, nHeight};
	UpdateRealPosition();
	mDisplayingHolder.Update();
	mAreaDelimiter.Clear();
}

void Canvas::OpenFile(SDL_Renderer *pRenderer, const char *pLoadFile, SDL_Point imageSize){
	mpImage->AddFileAsLayer(pRenderer, pLoadFile, imageSize);
	AppendCommand("52_S_SliderMax/"+std::to_string(mpImage->GetTotalLayers()-1)+"_InitialValue/"+std::to_string(mpImage->GetLayer())+"_"); //Refers to the slider SELECT_LAYER
	UpdateLayerOptions();

	//We can use GetCurrentSurface and GetLayer, since AddLayer also changes the current layer to the one just created
	mActionsManager.SetOriginalLayer(mpImage->GetCurrentSurface(), mpImage->GetLayer());
	mActionsManager.SetLayerCreation();
	
	mDimensions = {0, 0, imageSize.x, imageSize.y};
	UpdateRealPosition();
	mDisplayingHolder.Update();
}

SDL_Color Canvas::GetColor(){
	return mDrawColor;
}

void Canvas::SetColor(SDL_Color nDrawColor){
	mDrawColor = nDrawColor;
}

void Canvas::SetRadius(int nRadius){
	if(nRadius < 1){
		nRadius = 1;
	}

	//If the radius hasn't changed, we don't need to update the preview or circle
	if(nRadius-1 != tool_circle_data::radius){
		tool_circle_data::radius = nRadius-1;
		tool_circle_data::needsUpdate = true;
	}
}

int Canvas::GetRadius(){
	return tool_circle_data::radius+1;
}

void Canvas::DrawPixel(SDL_Point localPixel){
	SDL_Rect usedArea{0, 0, 0, 0};

	std::vector<SDL_Point> pixels{localPixel};
	switch(mUsedTool){
		case Tool::DRAW_TOOL:
			mPencil.ApplyOn(pixels, mDrawColor, mpImage->GetCurrentSurface(), &usedArea);
			break;
		case Tool::ERASE_TOOL:
			mEraser.ApplyOn(pixels, mpImage->GetCurrentSurface(), &usedArea);
			break;
		default:
			ErrorPrint("mUsedTool can't have the value "+std::to_string(static_cast<int>(mUsedTool)));
			break;
	}

	if(usedArea.w != 0){ //Theoretically if width is 0, height should also be 0, so no need to check
		mpImage->UpdateTexture(usedArea);
	}

	mLastMousePixel = localPixel;
	mActionsManager.pointTracker.push_back(mLastMousePixel);
}

void Canvas::DrawPixels(const std::vector<SDL_Point> &localPixels){
	SDL_Rect usedArea{0, 0, 0, 0};
	
	std::vector<SDL_Point> pixels(localPixels);
	switch(mUsedTool){
		case Tool::DRAW_TOOL:
			mPencil.ApplyOn(pixels, mDrawColor, mpImage->GetCurrentSurface(), &usedArea);
			break;
		case Tool::ERASE_TOOL:
			mEraser.ApplyOn(pixels, mpImage->GetCurrentSurface(), &usedArea);
			break;
		case Tool::COLOR_PICKER:
			ErrorPrint("mUsedTool shouldn't have the value "+std::to_string(static_cast<int>(mUsedTool))+ " when calling this method");
			break;
		default:
			ErrorPrint("mUsedTool can't have the value "+std::to_string(static_cast<int>(mUsedTool)));
			break;
	}

	if(usedArea.w != 0){ //Theoretically if width is 0, height should also be 0, so no need to check
		mpImage->UpdateTexture(usedArea);
	}

    mLastMousePixel = localPixels.back();
	mActionsManager.pointTracker.insert(mActionsManager.pointTracker.end(), localPixels.begin(), localPixels.end());
}

void Canvas::Clear(std::optional<SDL_Color> clearColor){
	mActionsManager.SetOriginalLayer(mpImage->GetCurrentSurface(), mpImage->GetLayer());
	
	if (clearColor.has_value()) {
		mpImage->Clear(clearColor.value());
	} else {
		mpImage->Clear(mDrawColor);
	}
	
	mActionsManager.SetChange({0, 0, mpImage->GetWidth(), mpImage->GetHeight()}, mpImage->GetCurrentSurface());
}

void Canvas::SetSavePath(const char *nSavePath){
	mSavePath = nSavePath;
}

void Canvas::SetOffset(int offsetX, int offsetY){
	mDimensions.x = offsetX;
	mDimensions.y = offsetY;
	mDisplayingHolder.Update();
	UpdateRealPosition();
}

void Canvas::SetResolution(float nResolution){
	//if(nResolution < M_MIN_RESOLUTION || nResolution > M_MAX_RESOLUTION) return;
	mResolution = std::clamp(nResolution, M_MIN_RESOLUTION, M_MAX_RESOLUTION);
	
	switch(mUsedTool){
		case Tool::DRAW_TOOL:
			mPencil.SetResolution(mResolution); 
			break;
		case Tool::ERASE_TOOL:
			mEraser.SetResolution(mResolution);
			break;
		case Tool::COLOR_PICKER:
			mColorPicker.SetResolution(mResolution);
			break;
		case Tool::AREA_DELIMITER:
			mAreaDelimiter.SetResolution(mResolution);
			break;
		default:
			ErrorPrint("mUsedTool can't have the value "+std::to_string(static_cast<int>(mUsedTool)));
			break;
	}

	int nWidth = mpImage->GetWidth()*mResolution, nHeight = mpImage->GetHeight()*mResolution;

	mRealPosition.x = viewport.w/2 - (viewport.w/2-(mRealPosition.x)) * nWidth * 1.0f / mDimensions.w;
	mRealPosition.y = viewport.h/2 - (viewport.h/2-(mRealPosition.y)) * nHeight * 1.0f / mDimensions.h;
	mDimensions.x = (int)(mRealPosition.x);
	mDimensions.y = (int)(mRealPosition.y);
	mDimensions.w = nWidth;
	mDimensions.h = nHeight;
	mDisplayingHolder.Update();
}

void Canvas::SetTool(Tool nUsedTool){
	mUsedTool = nUsedTool;

	switch(mUsedTool){
		case Tool::DRAW_TOOL:
			mPencil.Activate(); 
			AppendCommand("T_"+std::to_string(std::to_underlying(Tool::ERASE_TOOL))+"_Active/F_\n" +
						  "T_"+std::to_string(std::to_underlying(Tool::COLOR_PICKER))+"_Active/F_\n" + 
						  "T_"+std::to_string(std::to_underlying(Tool::AREA_DELIMITER))+"_Active/F_\n" + 
						  "T_"+std::to_string(std::to_underlying(Tool::DRAW_TOOL))+"_Active/T_");
			break;
		case Tool::ERASE_TOOL:
			mEraser.Activate();
			AppendCommand("T_"+std::to_string(std::to_underlying(Tool::DRAW_TOOL))+"_Active/F_\n" +
						  "T_"+std::to_string(std::to_underlying(Tool::COLOR_PICKER))+"_Active/F_\n" + 
						  "T_"+std::to_string(std::to_underlying(Tool::AREA_DELIMITER))+"_Active/F_\n" + 
						  "T_"+std::to_string(std::to_underlying(Tool::ERASE_TOOL))+"_Active/T_");
			break;
		case Tool::COLOR_PICKER:
			mColorPicker.Activate();
			AppendCommand("T_"+std::to_string(std::to_underlying(Tool::DRAW_TOOL))+"_Active/F_\n" +
						  "T_"+std::to_string(std::to_underlying(Tool::ERASE_TOOL))+"_Active/F_\n" + 
						  "T_"+std::to_string(std::to_underlying(Tool::AREA_DELIMITER))+"_Active/F_\n" + 
						  "T_"+std::to_string(std::to_underlying(Tool::COLOR_PICKER))+"_Active/T_");
			break;
		case Tool::AREA_DELIMITER:
			mAreaDelimiter.Activate();
			AppendCommand("T_"+std::to_string(std::to_underlying(Tool::DRAW_TOOL))+"_Active/F_\n" +
						  "T_"+std::to_string(std::to_underlying(Tool::ERASE_TOOL))+"_Active/F_\n" + 
						  "T_"+std::to_string(std::to_underlying(Tool::COLOR_PICKER))+"_Active/F_\n" +
						  "T_"+std::to_string(std::to_underlying(Tool::AREA_DELIMITER))+"_Active/T_");
			break;
		default:
			ErrorPrint("mUsedTool can't have the value "+std::to_string(static_cast<int>(mUsedTool)));
			mUsedTool = Tool::DRAW_TOOL;
			mPencil.Activate(); 
			break;
	}
}

void Canvas::ApplyAreaOutline(){
	std::vector<SDL_Point> pixels {};
	std::vector<SDL_FPoint> corners = mAreaDelimiter.GetPointsCopy();

	if(corners.empty()) return;

	for(int i = 0; i+1 < corners.size(); ++i){
		std::vector<SDL_Point> segmentPoints = GetPointsInFSegment(corners[i], corners[i+1]);
		pixels.insert(pixels.end(), segmentPoints.begin(), segmentPoints.end());
	}

	if(mAreaDelimiter.loopBack){
		std::vector<SDL_Point> segmentPoints = GetPointsInFSegment(corners.back(), corners.front());
		pixels.insert(pixels.end(), segmentPoints.begin(), segmentPoints.end());
	}

	Tool usedTool = mUsedTool;
	SetTool(Tool::DRAW_TOOL);
	
	mActionsManager.SetOriginalLayer(mpImage->GetCurrentSurface(), mpImage->GetLayer());

	DrawPixels(pixels);

	SDL_FRect enclosingRect = {-1,-1,-1,-1};
	SDL_Rect affectedRect;
	SDL_EncloseFPoints(corners.data(), corners.size(), nullptr, &enclosingRect);
	
	int radius = GetRadius();

	affectedRect.x = std::max((int)floorf(enclosingRect.x)+1-radius, 0);
	affectedRect.y = std::max((int)floorf(enclosingRect.y)+1-radius, 0);
	//To the width and heigth, we substract the offset of the x and y, so, for example, if 2 rows of pixels fell above the canvas, the height should be reduced by 2
	affectedRect.w = std::min((int)ceilf(enclosingRect.w)+1+radius - (affectedRect.x - (int)floorf(enclosingRect.x)+1-radius), mpImage->GetWidth());
	affectedRect.h = std::min((int)ceilf(enclosingRect.h)+1+radius - (affectedRect.y - (int)floorf(enclosingRect.y)+1-radius), mpImage->GetHeight());

	mActionsManager.SetChange(affectedRect, mpImage->GetCurrentSurface());
	mActionsManager.pointTracker.clear();

	SetTool(usedTool);
}

void Canvas::AppendCommand(const std::string &nCommand){
	mCommands += nCommand + "\n";
}

std::string Canvas::GiveCommands(){
	std::string sCopy(mCommands);
	mCommands.clear();
	return sCopy;
}

void Canvas::Undo(){
	//We don't want to undo anything if the user is drawing
	if(mHolded) return;

	int neededLayer = mActionsManager.GetUndoLayer();
	SDL_Rect affectedRect = {-1,-1,-1,-1};

	switch(mActionsManager.GetUndoType()){
		case ActionsManager::Action::STROKE:
			//We undo the changes, making sure that something actually changed
			if(mActionsManager.UndoChange(mpImage->GetSurfaceAtLayer(neededLayer), &affectedRect)){
				//If the layer changed is not from the current one, we set it
				if(mpImage->GetLayer() != neededLayer){
					mpImage->SetLayer(neededLayer);
					AppendCommand("52_S_InitialValue/"+std::to_string(mpImage->GetLayer())+"_"); //Refers to the slider SELECT_LAYER
				}
				
				//Finally we update the texture as needed
				mpImage->UpdateTexture(affectedRect);
			}
			break;
		
		case ActionsManager::Action::LAYER_CREATION:
			//First, we make sure to select the layer, and then delete it
			if(mpImage->GetLayer() != neededLayer){
				mpImage->SetLayer(neededLayer);
			}

			mpImage->DeleteCurrentLayer();
			AppendCommand("52_S_SliderMax/"+std::to_string(mpImage->GetTotalLayers()-1)+"_InitialValue/"+std::to_string(mpImage->GetLayer())+"_"); //Refers to the slider SELECT_LAYER
			
			mActionsManager.UndoChange(nullptr, nullptr); //This does nothing apart from decrementing the undo index
			//We don't need to update the texture, since DeleteCurrentLayer already does it
			break;

		case ActionsManager::Action::LAYER_DESTRUCTION:
			//First, we make sure to select the previous layer, and then add a layer on top of it
			if(mpImage->GetLayer() != neededLayer-1){
				mpImage->SetLayer(neededLayer-1);
			}

			mpImage->AddLayer();
			AppendCommand("52_S_SliderMax/"+std::to_string(mpImage->GetTotalLayers()-1)+"_InitialValue/"+std::to_string(mpImage->GetLayer())+"_"); //Refers to the slider SELECT_LAYER

			//Finally we set the surface to the deleted one
			mActionsManager.UndoChange(mpImage->GetCurrentSurface(), &affectedRect);
			//We need to update the texture since, even though AddLayer already does it, we then apply the changes of the destroyed layer
			mpImage->UpdateTexture(affectedRect);
			break;
			
		default: break;
	}
	UpdateLayerOptions();
}

void Canvas::Redo(){
	//We don't want to redo anything if the user is drawing
	if(mHolded) return;

	int neededLayer = mActionsManager.GetRedoLayer();
	SDL_Rect affectedRect = {-1,-1,-1,-1};

	switch(mActionsManager.GetRedoType()){
		case ActionsManager::Action::STROKE:
			//We undo the changes, making sure that something actually changed
			if(mActionsManager.RedoChange(mpImage->GetSurfaceAtLayer(neededLayer), &affectedRect)){
				//If the layer changed is not from the current one, we set it
				if(mpImage->GetLayer() != neededLayer){
					mpImage->SetLayer(neededLayer);
					AppendCommand("52_S_InitialValue/"+std::to_string(mpImage->GetLayer())+"_"); //Refers to the slider SELECT_LAYER
				}
				
				//Finally we update the texture as needed
				mpImage->UpdateTexture(affectedRect);
			}
			break;

		case ActionsManager::Action::LAYER_CREATION:
			//First, we make sure to select the previous layer, and then add a layer on top of it
			if(mpImage->GetLayer() != neededLayer-1){
				mpImage->SetLayer(neededLayer-1);
			}

			mpImage->AddLayer();
			AppendCommand("52_S_SliderMax/"+std::to_string(mpImage->GetTotalLayers()-1)+"_InitialValue/"+std::to_string(mpImage->GetLayer())+"_"); //Refers to the slider SELECT_LAYER

			//Finally we redo the surface
			mActionsManager.RedoChange(mpImage->GetCurrentSurface(), &affectedRect);
			//We need to update the texture since, even though AddLayer already does it, we then apply the changes of the layer
			mpImage->UpdateTexture(affectedRect);
			break;
		
		case ActionsManager::Action::LAYER_DESTRUCTION:
			//First, we make sure to select the layer, and then delete it
			if(mpImage->GetLayer() != neededLayer){
				mpImage->SetLayer(neededLayer);
			}

			mpImage->DeleteCurrentLayer();
			AppendCommand("52_S_SliderMax/"+std::to_string(mpImage->GetTotalLayers()-1)+"_InitialValue/"+std::to_string(mpImage->GetLayer())+"_"); //Refers to the slider SELECT_LAYER

			mActionsManager.RedoChange(nullptr, nullptr); //This does nothing apart from decrementing the undo index
			//We don't need to update the texture, since DeleteCurrentLayer already does it
			break;

		default: break;
	}
	UpdateLayerOptions();
}

void Canvas::HandleEvent(SDL_Event *event){
	if(event->type == SDL_MOUSEBUTTONDOWN){
        //Check the mouse is in the viewport
        SDL_Point mousePos = {event->button.x, event->button.y};
		if(!SDL_PointInRect(&mousePos, &viewport)) return;

		switch(mUsedTool){
			case Tool::DRAW_TOOL: case Tool::ERASE_TOOL:{
				mActionsManager.SetOriginalLayer(mpImage->GetCurrentSurface(), mpImage->GetLayer());

				SDL_Point pixel = GetPointCell({mousePos.x-(mDimensions.x+viewport.x), mousePos.y-(mDimensions.y+viewport.y)}, mResolution);
				DrawPixel(pixel);
				
				mHolded = true;
				break;
			}
			case Tool::COLOR_PICKER:{
				SDL_Point pixel = GetPointCell({mousePos.x-(mDimensions.x+viewport.x), mousePos.y-(mDimensions.y+viewport.y)}, mResolution);
				mColorPicker.GrabColor(this, mpImage.get(), pixel);
				break;
			}	
			case Tool::AREA_DELIMITER:{
				SDL_FPoint relativePosition = GetRealPointCell({mousePos.x-(mDimensions.x+viewport.x), mousePos.y-(mDimensions.y+viewport.y)}, mResolution);
				mHolded = mAreaDelimiter.HandleEvent(event, relativePosition);
				break;
			}
			default:
				ErrorPrint("mUsedTool can't have the value "+std::to_string(static_cast<int>(mUsedTool)));
				break;
		}
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
		if(ArePointsEqual(mLastMousePixel, pixel)) return;
		
		//If the last pixel where the mouse was registered and the current pixel are separated by more than 1 unit in any axis, we also set the pixels in between
		if(std::abs(pixel.x-mLastMousePixel.x) > 1 || std::abs(pixel.y-mLastMousePixel.y) > 1){
			
			switch(mUsedTool){
				case Tool::DRAW_TOOL: case Tool::ERASE_TOOL:
					DrawPixels(GetPointsInSegment(mLastMousePixel, pixel));
					break;

				case Tool::AREA_DELIMITER:
					SDL_FPoint relativePosition = GetRealPointCell({mousePos.x-(mDimensions.x+viewport.x), mousePos.y-(mDimensions.y+viewport.y)}, mResolution);
					mAreaDelimiter.HandleEvent(event, relativePosition);
					break;

				default:
					ErrorPrint("mUsedTool can't have the value "+std::to_string(static_cast<int>(mUsedTool)));
					break;
			}
		} else {
			switch(mUsedTool){
				case Tool::DRAW_TOOL: case Tool::ERASE_TOOL:
					DrawPixel(pixel);
					break;

				case Tool::AREA_DELIMITER:
					SDL_FPoint relativePosition = GetRealPointCell({mousePos.x-(mDimensions.x+viewport.x), mousePos.y-(mDimensions.y+viewport.y)}, mResolution);
					mAreaDelimiter.HandleEvent(event, relativePosition);
					break;

				default:
					ErrorPrint("mUsedTool can't have the value "+std::to_string(static_cast<int>(mUsedTool)));
					break;
			}
		}
	}
	else if (event->type == SDL_MOUSEBUTTONUP){
		mHolded = false;

		if(mUsedTool == Tool::AREA_DELIMITER){
			mAreaDelimiter.HandleEvent(event, {-1, -1}); //This one doesn't really matter
		}
		else if(mActionsManager.pointTracker.size() != 0){
			SDL_Rect enclosingRect = {-1,-1,-1,-1}, affectedRect;
			SDL_EnclosePoints(mActionsManager.pointTracker.data(), mActionsManager.pointTracker.size(), nullptr, &enclosingRect);
			
			int radius = GetRadius();

			affectedRect.x = std::max(enclosingRect.x+1-radius, 0);
			affectedRect.y = std::max(enclosingRect.y+1-radius, 0);
			//To the width and heigth, we substract the offset of the x and y, so, for example, if 2 rows of pixels fell above the canvas, the height should be reduced by 2
			affectedRect.w = std::min(enclosingRect.w+1+radius - (affectedRect.x - enclosingRect.x+1-radius), mpImage->GetWidth());
			affectedRect.h = std::min(enclosingRect.h+1+radius - (affectedRect.y - enclosingRect.y+1-radius), mpImage->GetHeight());
			
			mActionsManager.pointTracker.clear();

			//We make sure there is actually some change to apply
			if(affectedRect.w > 0 && affectedRect.h > 0) mActionsManager.SetChange(affectedRect, mpImage->GetCurrentSurface());
		}
	}
	else if(event->type == SDL_KEYDOWN){
		//We don't want to handle a key down if the text input is active, because that means a textfield is using the input
		if(SDL_IsTextInputActive()) return;

		switch(event->key.keysym.sym){
			//The 'wasd' controls may seem inverted, but you should imagine it as if you were controlling a camera, therefore the canvas moves in the opposite position
			case SDLK_a: mCanvasMovement |= Movement::RIGHT; break;
			case SDLK_d: mCanvasMovement |= Movement::LEFT; break;
			case SDLK_w: mCanvasMovement |= Movement::DOWN; break;
			case SDLK_s: mCanvasMovement |= Movement::UP; break;
			case SDLK_e: SetResolution(mResolution+10.0f*M_MIN_RESOLUTION); break;
			case SDLK_q: SetResolution(mResolution-10.0f*M_MIN_RESOLUTION); break;
			case SDLK_0: Undo(); break;
			case SDLK_9: Redo(); break;
			case SDLK_r: if(mUsedTool == Tool::AREA_DELIMITER && !mHolded) mAreaDelimiter.AddBeforeSelected(); break;
			case SDLK_f: if(mUsedTool == Tool::AREA_DELIMITER && !mHolded) mAreaDelimiter.EraseSelected(); break;
			case SDLK_c: if(mUsedTool == Tool::AREA_DELIMITER && !mHolded) mAreaDelimiter.Clear(); break;
		}
	}
	else if (event->type == SDL_KEYUP){
		//We don't want to handle a key down if the text input is active, because that means a textfield is using the input
		if(SDL_IsTextInputActive()) return;

		switch(event->key.keysym.sym){
			case SDLK_a: mCanvasMovement &= ~Movement::RIGHT; break;
			case SDLK_d: mCanvasMovement &= ~Movement::LEFT; break;
			case SDLK_w: mCanvasMovement &= ~Movement::DOWN; break;
			case SDLK_s: mCanvasMovement &= ~Movement::UP; break;
		}
	}
	else if (event->type == SDL_MOUSEWHEEL){
		//TODO: Call change function assigned, this makes it so you can customize what value does the mouse wheel change
		//It should also check the wheel is used inside the canvas viewport (no need to be in the canvas exactly), as otherwise it should operate on the hovered text if any

		//No need to clamp the value, as SetHardness already does that
		//mPencil.SetHardness(mPencil.GetHardness() + event->wheel.y*0.1f);
	}
}

void Canvas::Update(float deltaTime){
	mpImage->UpdateTexture();

	if(mCanvasMovement != Movement::NONE){
		float speed = ((SDL_GetModState() & KMOD_SHIFT) ? fastMovementSpeed : defaultMovementSpeed);

		if(mCanvasMovement & Movement::LEFT)	mRealPosition.x -= deltaTime * speed;
		if(mCanvasMovement & Movement::RIGHT)	mRealPosition.x += deltaTime * speed;
		if(mCanvasMovement & Movement::UP)		mRealPosition.y -= deltaTime * speed;
		if(mCanvasMovement & Movement::DOWN)	mRealPosition.y += deltaTime * speed;

		mDimensions.x = (int)mRealPosition.x;
		mDimensions.y = (int)mRealPosition.y;
		mDisplayingHolder.Update();
	}
}

void Canvas::DrawIntoRenderer(SDL_Renderer *pRenderer){
	SDL_RenderSetViewport(pRenderer, &viewport);
	
	//Gives a shade to the image, so it is easily distinguishable from the background, works better with birght colors.
	//This isn't handled by the 'DisplayingHolder' since the backgroundColor (at least for now) is constantly changing
	for(int border = 1; border <= DisplayingHolder::MAX_BORDER; border++){
		SDL_Rect imageShade = {mDimensions.x - border, mDimensions.y - border, mDimensions.w + 2*border, mDimensions.h + 2*border};

		SDL_Color shadeColor = {(Uint8)(backgroundColor.r*border/(float)DisplayingHolder::MAX_BORDER),
								(Uint8)(backgroundColor.g*border/(float)DisplayingHolder::MAX_BORDER), 
								(Uint8)(backgroundColor.b*border/(float)DisplayingHolder::MAX_BORDER)};

		SDL_SetRenderDrawColor(pRenderer, shadeColor.r, shadeColor.g, shadeColor.b, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(pRenderer, &imageShade);
	}

	SDL_SetRenderDrawColor(pRenderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRects(pRenderer, mDisplayingHolder.backgroundRects, 4);

	SDL_RenderSetViewport(pRenderer, &mDisplayingHolder.squaresViewport);
	SDL_SetRenderDrawColor(pRenderer, mDisplayingHolder.grey[0].r, mDisplayingHolder.grey[0].g, mDisplayingHolder.grey[0].b, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRects(pRenderer, mDisplayingHolder.lightGreySquares.data(), mDisplayingHolder.lightGreySquares.size());
	SDL_SetRenderDrawColor(pRenderer, mDisplayingHolder.grey[1].r, mDisplayingHolder.grey[1].g, mDisplayingHolder.grey[1].b, SDL_ALPHA_OPAQUE);
	SDL_RenderFillRects(pRenderer, mDisplayingHolder.darkGreySquares.data(), mDisplayingHolder.darkGreySquares.size());

	//SDL_Rect intersectionRect = {mDimensions.x + viewport.x, mDimensions.y + viewport.y, std::min(mDimensions.w, viewport.w), std::min(mDimensions.h, viewport.h)};
	//SDL_RenderSetViewport(pRenderer, &intersectionRect);
	//mpImage->DrawIntoRenderer(pRenderer, mDimensions);
	//SDL_Rect intersectRect = {std::max(mDimensions.x, viewport.x),std::max(mDimensions.x, viewport.x), mDimensions.w, mDimensions.h};
	//SDL_RenderSetViewport(pRenderer, &intersectRect);
	SDL_RenderSetViewport(pRenderer, &viewport);
	mpImage->DrawIntoRenderer(pRenderer, mDimensions);

	bool enoughRadius = false;
	switch(mUsedTool){
		case Tool::DRAW_TOOL: case Tool::ERASE_TOOL:
			enoughRadius = GetRadius() > 4;
			break;
		case Tool::COLOR_PICKER: case Tool::AREA_DELIMITER:
			enoughRadius = true;
			break;
		default:
			ErrorPrint("mUsedTool can't have the value "+std::to_string(static_cast<int>(mUsedTool)));
			enoughRadius = false;
			break;
	}
	
	SDL_Color areaDelimiterColor = toolPreviewMainColor; //We don't use preview color because we don't want it to change based on the mouse position
	areaDelimiterColor.a = 50;

	if(enoughRadius || !mHolded){
		SDL_Point rawMousePosition;
		SDL_GetMouseState(&rawMousePosition.x, &rawMousePosition.y);
		
		SDL_FPoint mousePosition;
		SDL_RenderWindowToLogical(pRenderer, rawMousePosition.x, rawMousePosition.y, &mousePosition.x, &mousePosition.y);

		SDL_Rect nViewport = {viewport.x + std::max(mDimensions.x, 0), viewport.y + std::max(mDimensions.y, 0), std::min(mDimensions.w, viewport.w), std::min(mDimensions.h, viewport.h)};
		SDL_RenderSetViewport(pRenderer, &nViewport);

		//We find the middle pixel
		SDL_Point pixel = GetPointCell({(int)mousePosition.x-viewport.x-mDimensions.x, (int)mousePosition.y-viewport.y-mDimensions.y}, mResolution);
		
		SDL_Color previewColor = toolPreviewMainColor;
		bool validColor = true;
		SDL_Color pixelColor = mpImage->GetPixelColor(pixel, &validColor);
		if(validColor && ((int)pixelColor.r + (int)pixelColor.g + (int)pixelColor.b)*(pixelColor.a/255.0f) <= 127.0f*3){
			previewColor = toolPreviewAlternateColor;
		}

        /*
        We would perform these operations in order to then send pixel to Pencil::DrawPreview. This has been discarded as it wouldn't result in a pixel perfect preview 	
		pixel.x *= mResolution; pixel.y *= mResolution;
		*/

		//We find the middle pixel
		SDL_Point mouseToCanvas = {(int)mousePosition.x - nViewport.x  + viewport.x, (int)mousePosition.y - nViewport.y + viewport.y};
		previewColor.a = 50;

		switch(mUsedTool){
			case Tool::DRAW_TOOL:
				mPencil.DrawPreview(mouseToCanvas, pRenderer, previewColor);
				break;
			case Tool::ERASE_TOOL:
				mEraser.DrawPreview(mouseToCanvas, pRenderer, previewColor);
				break;
			case Tool::COLOR_PICKER:
				mColorPicker.DrawPreview(mouseToCanvas, pRenderer, previewColor);
				break;
			case Tool::AREA_DELIMITER:
				SDL_RenderSetViewport(pRenderer, &viewport);
				mAreaDelimiter.DrawPreview({mDimensions.x, mDimensions.y}, pRenderer, areaDelimiterColor);
				break;
			default:
				ErrorPrint("mUsedTool can't have the value "+std::to_string(static_cast<int>(mUsedTool)));
				break;
		};
	}

	SDL_RenderSetViewport(pRenderer, &viewport);
	mAreaDelimiter.DrawArea({mDimensions.x, mDimensions.y}, pRenderer, areaDelimiterColor);

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
	mDisplayingHolder.Update();
	UpdateRealPosition();
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

void Canvas::AddLayer(){
	if(mHolded) return; //We don't want to change the current layer if its being used

	mpImage->AddLayer();

	//We can use GetCurrentSurface and GetLayer, since AddLayer also changes the current layer to the one just created
	mActionsManager.SetOriginalLayer(mpImage->GetCurrentSurface(), mpImage->GetLayer());
	mActionsManager.SetLayerCreation();
	
	UpdateLayerOptions();
}

void Canvas::DeleteCurrentLayer(){
	if(mHolded) return; //We don't want the current layer to get deleted if its being used

	mActionsManager.SetOriginalLayer(mpImage->GetCurrentSurface(), mpImage->GetLayer());

	//We need to make sure that the current layer can be deleted, otherwise we would be adding an event that really didn't ocurr
	if(mpImage->DeleteCurrentLayer()){
		mActionsManager.SetLayerDestruction();
	}

	UpdateLayerOptions();
}

void Canvas::SetLayer(int nLayer){
	if(mHolded) return; //We don't want to change the current layer if its being used

	mpImage->SetLayer(nLayer);

	UpdateLayerOptions();
}

void Canvas::SetLayerVisibility(bool visible){
	if(mHolded) return; //We don't want to make the current layer visible or hiden

	mpImage->SetLayerVisibility(visible);
}

void Canvas::SetLayerAlpha(Uint8 alpha){
	if(mHolded) return; //We don't want to make the current layer visible or hiden

	mpImage->SetLayerAlpha(alpha);
}

MutableTexture *Canvas::GetImage(){
	return mpImage.get();
}

//DISPLAYING HOLDER METHODS:

Canvas::DisplayingHolder::DisplayingHolder(Canvas *npOwner) : mpOwner(npOwner){}

void Canvas::DisplayingHolder::Update(){
	SDL_Rect dimensions = mpOwner->mDimensions, viewport = mpOwner->viewport;
	//Some MAX_BORDERs cancel out but I'd rather keep it like this so it's more understandable. This is more efficient that filling the whole viewport
	backgroundRects[0] = {0, 0, dimensions.x - MAX_BORDER, viewport.h};//LEFT RECT
	backgroundRects[1] = {dimensions.x - MAX_BORDER, 0, dimensions.w + 2*MAX_BORDER, dimensions.y - MAX_BORDER};//TOP RECT
	backgroundRects[2] = {dimensions.x - MAX_BORDER, dimensions.y - MAX_BORDER + dimensions.h + 2 * MAX_BORDER, dimensions.w + 2*MAX_BORDER, viewport.h - dimensions.h - (dimensions.y - MAX_BORDER)}; //BOTTOM RECT
	backgroundRects[3] = {dimensions.x - MAX_BORDER + dimensions.w + 2*MAX_BORDER, 0, viewport.w - dimensions.w - (dimensions.x - MAX_BORDER), viewport.h}; //RIGHT RECT

	int xOffset = std::min(dimensions.x, 0), yOffset = std::min(dimensions.y, 0);
	SDL_IntersectRect(&dimensions, &viewport, &squaresViewport);
	squaresViewport = {std::max(dimensions.x, 0) + viewport.x, std::max(dimensions.y, 0) + viewport.y, dimensions.w + xOffset, dimensions.h + yOffset};

	const int MAX_X_SQUARES = 10, MAX_Y_SQUARES = ceil(MAX_X_SQUARES*dimensions.h*1.0f/dimensions.w), SQUARE_SIZE = ceil(dimensions.w*1.0f/MAX_X_SQUARES);

	lightGreySquares.clear(); darkGreySquares.clear();
	//This works as long as MAX_X_SQUARES is even (would also work if MAX_Y_SQUARES is even)
	lightGreySquares.reserve(MAX_X_SQUARES * MAX_Y_SQUARES / 2); darkGreySquares.reserve(MAX_X_SQUARES * MAX_Y_SQUARES / 2);
	SDL_Color grey[2] = {SDL_Color{205, 205, 205}, SDL_Color{155, 155, 155}};

	int index = 0;
	for(int squareX = 0; squareX < MAX_X_SQUARES; squareX++){
		SDL_Rect squareRect = {squareX * SQUARE_SIZE + xOffset, yOffset, SQUARE_SIZE, SQUARE_SIZE};
		for(int squareY = 0; squareY < MAX_Y_SQUARES; squareY++){
			if(index == 1){ //We alternate between dark grey and light grey squares
				index = 0;
				darkGreySquares.push_back(squareRect);
			} else {
				index = 1;
				lightGreySquares.push_back(squareRect);
			}

			squareRect.y += SQUARE_SIZE;
		}
		if(MAX_Y_SQUARES%2 == 0) index++; //Causes the greys to alternate
	}
}

//ACTIONS MANAGER METHODS:

void Canvas::ActionsManager::Initialize(int nMaxUndoActions){
	mMaxActionsAmount = std::max(nMaxUndoActions, 0);
	mChangedRects.resize(nMaxUndoActions);
	mInitialSurface.resize(nMaxUndoActions);
	mEndingSurface.resize(nMaxUndoActions);
}

void Canvas::ActionsManager::SetOriginalLayer(SDL_Surface *pSurfaceToCopy, int surfaceLayer){
	mpOriginalLayer.reset(SDL_ConvertSurface(pSurfaceToCopy, pSurfaceToCopy->format, 0));
	mOriginalLayer = surfaceLayer;
}

void Canvas::ActionsManager::SetChange(SDL_Rect affectedRegion, SDL_Surface *pResultingSurface){
	RotateUndoHistoryIfFull();

	mActionIndex++;
	mCurrentMaxIndex = mActionIndex;
	mChangedRects[mActionIndex] = {affectedRegion, mOriginalLayer};

	//Despite the formats coming from different sources, it's assumed that both are the same
	mInitialSurface[mActionIndex].reset(SDL_CreateRGBSurfaceWithFormat(0, affectedRegion.w, affectedRegion.h, 0, mpOriginalLayer->format->format));
	mEndingSurface[mActionIndex].reset(SDL_CreateRGBSurfaceWithFormat(0, affectedRegion.w, affectedRegion.h, 0, pResultingSurface->format->format));

	Uint8 initialAlphaMod = 0;
	Uint8 resultingAlphaMod = 0;
	SDL_BlendMode resultingBlendMode;

	SDL_GetSurfaceAlphaMod(mpOriginalLayer.get(), &initialAlphaMod);
	SDL_GetSurfaceAlphaMod(pResultingSurface, &resultingAlphaMod);
	SDL_GetSurfaceBlendMode(pResultingSurface, &resultingBlendMode);

	SDL_SetSurfaceAlphaMod(mpOriginalLayer.get(), SDL_ALPHA_OPAQUE);
	SDL_SetSurfaceBlendMode(mpOriginalLayer.get(), SDL_BLENDMODE_NONE);
	SDL_BlitSurface(mpOriginalLayer.get(), &affectedRegion, mInitialSurface[mActionIndex].get(), nullptr);
	SDL_SetSurfaceAlphaMod(mpOriginalLayer.get(), initialAlphaMod);

	SDL_SetSurfaceAlphaMod(pResultingSurface, SDL_ALPHA_OPAQUE);
	SDL_SetSurfaceBlendMode(pResultingSurface, SDL_BLENDMODE_NONE);
	SDL_BlitSurface(pResultingSurface, &affectedRegion, mEndingSurface[mActionIndex].get(), nullptr);
	SDL_SetSurfaceAlphaMod(pResultingSurface, resultingAlphaMod);
	SDL_SetSurfaceBlendMode(pResultingSurface, resultingBlendMode);

	//This is done preemtively, in case of any future undo/redo
	SDL_SetSurfaceBlendMode(mInitialSurface[mActionIndex].get(), SDL_BLENDMODE_NONE);
	SDL_SetSurfaceBlendMode(mEndingSurface[mActionIndex].get(), SDL_BLENDMODE_NONE);
	SDL_SetSurfaceAlphaMod(mInitialSurface[mActionIndex].get(), initialAlphaMod);
	SDL_SetSurfaceAlphaMod(mEndingSurface[mActionIndex].get(), resultingAlphaMod);
}

void Canvas::ActionsManager::ClearRedoData(){
	for(int i = mActionIndex+1; i <= mCurrentMaxIndex; i++){
		//We don't clear the layered rects since this wouldn't make them take less memory
		mInitialSurface[i].reset();
		mEndingSurface[i].reset();
	}
	mCurrentMaxIndex = mActionIndex;
}

void Canvas::ActionsManager::ClearData(){
	pointTracker.clear();
	mActionIndex = -1;
	mCurrentMaxIndex = -1;
	mpOriginalLayer.reset();
	mOriginalLayer = -1;
	mChangedRects.clear(); mChangedRects.resize(mMaxActionsAmount);
	mInitialSurface.clear(); mInitialSurface.resize(mMaxActionsAmount);
	mEndingSurface.clear(); mEndingSurface.resize(mMaxActionsAmount);
}

void Canvas::ActionsManager::SetLayerCreation(){
	RotateUndoHistoryIfFull();
	
	mActionIndex++;
	mCurrentMaxIndex = mActionIndex;
	mChangedRects[mActionIndex] = {{0, 0, mpOriginalLayer->w, mpOriginalLayer->h}, mOriginalLayer};
	mInitialSurface[mActionIndex].reset(nullptr);
	mEndingSurface[mActionIndex].reset(SDL_ConvertSurface(mpOriginalLayer.get(), mpOriginalLayer->format, 0));
	
	SDL_SetSurfaceBlendMode(mEndingSurface[mActionIndex].get(), SDL_BLENDMODE_NONE);
}

void Canvas::ActionsManager::SetLayerDestruction(){
	RotateUndoHistoryIfFull();
	
	mActionIndex++;
	mCurrentMaxIndex = mActionIndex;
	mChangedRects[mActionIndex] = {{0, 0, mpOriginalLayer->w, mpOriginalLayer->h}, mOriginalLayer};
	mInitialSurface[mActionIndex].reset(SDL_ConvertSurface(mpOriginalLayer.get(), mpOriginalLayer->format, 0));
	mEndingSurface[mActionIndex].reset(nullptr);
	
	SDL_SetSurfaceBlendMode(mInitialSurface[mActionIndex].get(), SDL_BLENDMODE_NONE);
}

int Canvas::ActionsManager::GetUndoLayer(){
	return mChangedRects[mActionIndex].layer;
}

Canvas::ActionsManager::Action Canvas::ActionsManager::GetUndoType(){
	if(mActionIndex == -1){
		return Action::NONE;
	}

	if(mInitialSurface[mActionIndex].get() == nullptr){
		return Action::LAYER_CREATION;
	}
	if(mEndingSurface[mActionIndex].get() == nullptr){
		return Action::LAYER_DESTRUCTION;
	}

	return Action::STROKE;
}

bool Canvas::ActionsManager::UndoChange(SDL_Surface *pSurfaceToUndo, SDL_Rect *undoneRegion){
	if(mActionIndex == -1) return false;

	if(undoneRegion != nullptr){
		*undoneRegion = mChangedRects[mActionIndex].rect;
	}

	Uint8 alphaMod = 0;

	switch(GetUndoType()){
		case Action::STROKE:
			SDL_GetSurfaceAlphaMod(mInitialSurface[mActionIndex].get(), &alphaMod);
			SDL_SetSurfaceAlphaMod(mInitialSurface[mActionIndex].get(), SDL_ALPHA_OPAQUE);

			SDL_BlitSurface(mInitialSurface[mActionIndex].get(), nullptr, pSurfaceToUndo, &mChangedRects[mActionIndex].rect);

			SDL_SetSurfaceAlphaMod(mInitialSurface[mActionIndex].get(), alphaMod);
			SDL_SetSurfaceAlphaMod(pSurfaceToUndo, alphaMod);
			mActionIndex--;
			return true;
		
		case Action::LAYER_CREATION: 
			//Undoing the creation of layers should be managed on the user end, using GetUndoType before calling this method
			mActionIndex--;
			return true;

		case Action::LAYER_DESTRUCTION:
			SDL_GetSurfaceAlphaMod(mInitialSurface[mActionIndex].get(), &alphaMod);
			SDL_SetSurfaceAlphaMod(mInitialSurface[mActionIndex].get(), SDL_ALPHA_OPAQUE);

			//It's assumed that 'pSurfaceToUndo' points to a new surface, corresponding to the layer that was destroyed
			SDL_BlitSurface(mInitialSurface[mActionIndex].get(), nullptr, pSurfaceToUndo, &mChangedRects[mActionIndex].rect);
			
			SDL_SetSurfaceAlphaMod(mInitialSurface[mActionIndex].get(), alphaMod);
			SDL_SetSurfaceAlphaMod(pSurfaceToUndo, alphaMod);
			
			mActionIndex--;
			return true;
		
		case Action::NONE: default: return false;
	}
}


int Canvas::ActionsManager::GetRedoLayer(){
	return mChangedRects[mActionIndex+1].layer;
}

Canvas::ActionsManager::Action Canvas::ActionsManager::GetRedoType(){
	if(mActionIndex == mCurrentMaxIndex){
		return Action::NONE;
	}

	if(mInitialSurface[mActionIndex+1].get() == nullptr){
		return Action::LAYER_CREATION;
	}
	if(mEndingSurface[mActionIndex+1].get() == nullptr){
		return Action::LAYER_DESTRUCTION;
	}

	return Action::STROKE;
}

bool Canvas::ActionsManager::RedoChange(SDL_Surface *pSurfaceToRedo, SDL_Rect *redoneRegion){
	if(mActionIndex == mCurrentMaxIndex) return false;

	if(redoneRegion != nullptr){
		*redoneRegion = mChangedRects[mActionIndex+1].rect;
	}

	Uint8 alphaMod = 0;

	switch(GetRedoType()){
		case Action::STROKE:
			SDL_GetSurfaceAlphaMod(mEndingSurface[mActionIndex+1].get(), &alphaMod);
			SDL_SetSurfaceAlphaMod(mEndingSurface[mActionIndex+1].get(), SDL_ALPHA_OPAQUE);

			SDL_BlitSurface(mEndingSurface[mActionIndex+1].get(), nullptr, pSurfaceToRedo, &mChangedRects[mActionIndex+1].rect);
			
			SDL_SetSurfaceAlphaMod(mEndingSurface[mActionIndex+1].get(), alphaMod);
			SDL_SetSurfaceAlphaMod(pSurfaceToRedo, alphaMod);
			mActionIndex++;
			return true;
		
		case Action::LAYER_CREATION: 
			SDL_GetSurfaceAlphaMod(mEndingSurface[mActionIndex+1].get(), &alphaMod);
			SDL_SetSurfaceAlphaMod(mEndingSurface[mActionIndex+1].get(), SDL_ALPHA_OPAQUE);

			//It's assumed that 'pSurfaceToRedo' points to a new surface, corresponding to the layer that needs to be created
			SDL_BlitSurface(mEndingSurface[mActionIndex+1].get(), nullptr, pSurfaceToRedo, &mChangedRects[mActionIndex+1].rect);
			
			SDL_SetSurfaceAlphaMod(mEndingSurface[mActionIndex+1].get(), alphaMod);
			SDL_SetSurfaceAlphaMod(pSurfaceToRedo, alphaMod);;
			mActionIndex++;
			return true;

		case Action::LAYER_DESTRUCTION:
			//The destruction of layers should be managed on the user end, using GetRedoType before calling this method
			mActionIndex++;
			return true;
		
		case Action::NONE: default: return false;
	}
}

void Canvas::ActionsManager::RotateUndoHistoryIfFull(){
	if(mActionIndex+1 >= mMaxActionsAmount){
		//We rotate everything one to the left, putting the oldest action to the front
		std::rotate(mChangedRects.begin(), mChangedRects.begin()+1, mChangedRects.end());
		std::rotate(mInitialSurface.begin(), mInitialSurface.begin()+1, mInitialSurface.end());
		std::rotate(mEndingSurface.begin(), mEndingSurface.begin()+1, mEndingSurface.end());

		//Then, we lower 'mActionIndex', resulting in the overwrite of the oldest action
		mActionIndex--;
	}
}

void Canvas::UpdateLayerOptions(){
	AppendCommand("53_T_InitialValue/"+std::string(mpImage->GetLayerVisibility() ? "T" : "F")+"_"); //Refers to the tick button SHOW_LAYER
	AppendCommand("54_S_InitialValue/"+std::to_string(mpImage->GetLayerAlpha())+"_"); //Refers to the slider LAYER_ALPHA
}