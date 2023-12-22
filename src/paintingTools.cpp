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

		for(int i = 0; i < mPreviewRects.size(); ++i){
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
					*basePixel = SDL_MapRGBA(pSurfaceToModify->format, SDL_ALPHA_OPAQUE*baseColor.r, SDL_ALPHA_OPAQUE*baseColor.g, SDL_ALPHA_OPAQUE*baseColor.b, SDL_ALPHA_OPAQUE*baseColor.a);
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
		//TODO: modify hardness system (mHardness = 1 doesn't make all the pixels fsully opaque (and (mRadius+1)*mHardness is a bit of an overkill))
		AlphaCalculation alphaCalculation = mAlphaCalculation;
		float hardness = mHardness;

		return [alphaCalculation, hardness](const SDL_Point& center, const SDL_Point& position) ->Uint8{
			float centerDistance = sqrt(pow(position.x-center.x, 2) + pow(position.y-center.y, 2));
			float maxDistance = tool_circle_data::radius*hardness;
			Uint8 alpha;
			
			if(centerDistance <= maxDistance){
				alpha = SDL_ALPHA_OPAQUE;
			}
			else switch(alphaCalculation){
				case AlphaCalculation::LINEAR: 
					alpha = (Uint8)(SDL_ALPHA_OPAQUE * (1-((centerDistance-maxDistance)/(1+tool_circle_data::radius-maxDistance))));
					break;
				case AlphaCalculation::QUADRATIC:
					alpha = (Uint8)(SDL_ALPHA_OPAQUE * (1-pow((centerDistance-maxDistance)/(1+tool_circle_data::radius-maxDistance), 2)));
					break;
				case AlphaCalculation::EXPONENTIAL:
					alpha = (Uint8)(SDL_ALPHA_OPAQUE * exp(-6*(centerDistance-maxDistance)/(1+tool_circle_data::radius-maxDistance)));
					break;
			}
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
		hexColor << std::hex << ((((unsigned int)color.r)<<16u) + (((unsigned int)color.g)<<8u) + (unsigned int)color.b);
		pCanvas->AppendCommand("0_H_InitialValue/"+hexColor.str()+"_");
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

//MUTABLE TEXTURE METHODS:

MutableTexture::MutableTexture(SDL_Renderer *pRenderer, int width, int height, SDL_Color fillColor){
	mpSurfaces.resize(1);
	mpSurfaces[mSelectedLayer].reset(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888));
    mpTexture.reset(SDL_CreateTexture(pRenderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, width, height));
	SDL_SetTextureBlendMode(mpTexture.get(), SDL_BLENDMODE_BLEND);

	Clear(fillColor);
}

MutableTexture::MutableTexture(SDL_Renderer *pRenderer, const char *pImage){
	SDL_Surface *loaded = IMG_Load(pImage);
	
	mpSurfaces.resize(1);
	mpSurfaces[mSelectedLayer].reset(SDL_ConvertSurfaceFormat(loaded, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888, 0));
    mpTexture.reset(SDL_CreateTexture(pRenderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, loaded->w, loaded->h));
	SDL_SetTextureBlendMode(mpTexture.get(), SDL_BLENDMODE_BLEND);

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

	SDL_Color pixelColor = {255, 255, 255, SDL_ALPHA_OPAQUE};
	
	//We calculate the end displayed color
	for(int i = 0; i < mpSurfaces.size(); ++i){
		SDL_Color auxiliar;
		SDL_GetRGBA(*UnsafeGetPixel(pixel, i), mpSurfaces[i]->format, &auxiliar.r, &auxiliar.g, &auxiliar.b, &auxiliar.a);
		ApplyColorToColor(pixelColor, auxiliar);
	}
	return pixelColor;
}

void MutableTexture::Clear(const SDL_Color &clearColor){
	//Currently only clears the current layer
	SDL_FillRect(mpSurfaces[mSelectedLayer].get(), nullptr, SDL_MapRGBA(mpSurfaces[mSelectedLayer]->format, clearColor.r, clearColor.g, clearColor.b, clearColor.a));

	UpdateWholeTexture();
}

void MutableTexture::ApplyColorToColor(SDL_Color &baseColor, const SDL_Color &appliedColor){
	FColor base {.r = baseColor.r/255.0f, .g = baseColor.g/255.0f, .b = baseColor.b/255.0f, .a = baseColor.a/255.0f};
	FColor applied{.r = appliedColor.r/255.0f, .g = appliedColor.g/255.0f, .b = appliedColor.b/255.0f, .a = appliedColor.a/255.0f};

	ApplyColorToColor(base, applied);

	baseColor.r = SDL_ALPHA_OPAQUE*(base.r);
	baseColor.g = SDL_ALPHA_OPAQUE*(base.g);
	baseColor.b = SDL_ALPHA_OPAQUE*(base.b);
	baseColor.a = SDL_ALPHA_OPAQUE*(base.a);
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
	for(const auto& surface : mpSurfaces){
		SDL_BlitSurface(surface.get(), &rect, texturesSurface, nullptr);
	}
	
	SDL_UnlockTexture(mpTexture.get());
}

void MutableTexture::AddLayer(SDL_Renderer *pRenderer){
	mpSurfaces.emplace(mpSurfaces.begin()+mSelectedLayer+1, SDL_CreateRGBSurfaceWithFormat(0, GetWidth(), GetHeight(), 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGBA8888));
    mSelectedLayer++;

	SDL_SetSurfaceBlendMode(mpSurfaces[mSelectedLayer].get(), SDL_BLENDMODE_BLEND);
	
	//Currently all new layers are created with no colors
	Clear({255, 255, 255, SDL_ALPHA_TRANSPARENT});
	UpdateWholeTexture();
}

void MutableTexture::DeleteCurrentLayer(){
	if(mpSurfaces.size() == 1){
		DebugPrint("Can't delete current layer, as it is the last one left");
		return;
	}

	//Better safe than sorry
	mSelectedLayer = std::clamp(mSelectedLayer, 0, (int)mpSurfaces.size()-1);

	mpSurfaces.erase(mpSurfaces.begin() + mSelectedLayer);
	
	UpdateWholeTexture();
	if(mSelectedLayer != 0) mSelectedLayer--;
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

	for(const auto& surface : mpSurfaces){
		SDL_BlitSurface(surface.get(), nullptr, pSaveSurface.get(), nullptr);
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

Canvas::Canvas(SDL_Renderer *pRenderer, int nWidth, int nHeight) : mpImage(new MutableTexture(pRenderer, nWidth, nHeight)), mDisplayingHolder(this){
	mDimensions = {0, 0, nWidth, nHeight};
	mDisplayingHolder.Update();
	UpdateRealPosition();
}

Canvas::Canvas(SDL_Renderer *pRenderer, const char *pLoadFile) : mpImage(new MutableTexture(pRenderer, pLoadFile)), mDisplayingHolder(this){
	mDimensions = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};
	mDisplayingHolder.Update();
	UpdateRealPosition();
}

Canvas::~Canvas(){
	if(saveOnDestroy) Save();
}

void Canvas::Resize(SDL_Renderer *pRenderer, int nWidth, int nHeight){
	mpImage.reset(new MutableTexture(pRenderer, nWidth, nHeight));
	mDimensions = {0, 0, nWidth, nHeight};
	mDisplayingHolder.Update();
	UpdateRealPosition();
}

void Canvas::OpenFile(SDL_Renderer *pRenderer, const char *pLoadFile){
	mpImage.reset(new MutableTexture(pRenderer, pLoadFile));
	mDimensions = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};
	mDisplayingHolder.Update();
	UpdateRealPosition();
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
	SDL_Rect usedArea{0, 0, 0, 0}, imageRect = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};

	std::vector<SDL_Point> pixels{localPixel};
	switch(mUsedTool){
		case Tool::DRAW_TOOL:
			mPencil.ApplyOn(pixels, mDrawColor, mpImage->GetCurrentSurface(), &usedArea);
			break;
		case Tool::ERASE_TOOL:
			mEraser.ApplyOn(pixels, mpImage->GetCurrentSurface(), &usedArea);
			break;
		default:
			ErrorPrint("mUsedTool can't have the value "+static_cast<int>(mUsedTool));
			break;
	}

	if(usedArea.w != 0){ //Theoretically if width is 0, height should also be 0, so no need to check
		mpImage->UpdateTexture(usedArea);
	}

	mLastMousePixel = localPixel;
}

void Canvas::DrawPixels(const std::vector<SDL_Point> &localPixels){
	SDL_Rect usedArea{0, 0, 0, 0}, imageRect = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};
	
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
			ErrorPrint("mUsedTool shouldn't have the value "+std::to_string(static_cast<int>(mUsedTool))+ " when calling this method");
			break;
		default:
			ErrorPrint("mUsedTool can't have the value "+static_cast<int>(mUsedTool));
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

	//TODO: reading the commands from files is a temporal solution and while it isn't that this shouldn't be possible, but for achieving things like only having active
	// certain kind of options inside a window, a TAGS system should be added, where options could be filtered by given tags
	switch(mUsedTool){
		case Tool::DRAW_TOOL:
			mPencil.Activate(); 
			AppendCommand(ReadFileToString("InternalData/ToolWindowPencil.txt"));
			break;
		case Tool::ERASE_TOOL:
			mEraser.Activate();
			AppendCommand(ReadFileToString("InternalData/ToolWindowEraser.txt"));
			break;
		case Tool::COLOR_PICKER:
			mColorPicker.Activate();
			AppendCommand(ReadFileToString("InternalData/ToolWindowColorpicker.txt"));
			break;
		default:
			ErrorPrint("mUsedTool can't have the value "+static_cast<int>(mUsedTool));
			mUsedTool = Tool::DRAW_TOOL;
			mPencil.Activate(); 
			break;
	}
}

void Canvas::AppendCommand(const std::string &nCommand){
	mCommands += nCommand + "\n";
}

std::string Canvas::GiveCommands(){
	std::string mCopy(mCommands);
	mCommands.clear();
	return mCopy;
}

void Canvas::HandleEvent(SDL_Event *event){
	if(event->type == SDL_MOUSEBUTTONDOWN){
        //Check the mouse is in the viewport
        SDL_Point mousePos = {event->button.x, event->button.y};
		if(!SDL_PointInRect(&mousePos, &viewport)) return;

		SDL_Point pixel = GetPointCell({mousePos.x-(mDimensions.x+viewport.x), mousePos.y-(mDimensions.y+viewport.y)}, mResolution);

		switch(mUsedTool){
			case Tool::DRAW_TOOL: case Tool::ERASE_TOOL:
				DrawPixel(pixel);
				mHolded = true;
				break;

			case Tool::COLOR_PICKER:
				mColorPicker.GrabColor(this, mpImage.get(), pixel);
				break;

			default:
				ErrorPrint("mUsedTool can't have the value "+static_cast<int>(mUsedTool));
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

				default:
					ErrorPrint("mUsedTool can't have the value "+static_cast<int>(mUsedTool));
					break;
			}
		} else {
			switch(mUsedTool){
				case Tool::DRAW_TOOL: case Tool::ERASE_TOOL:
					DrawPixel(pixel);
					break;

				default:
					ErrorPrint("mUsedTool can't have the value "+static_cast<int>(mUsedTool));
					break;
			}
		}
	}
	else if (event->type == SDL_MOUSEBUTTONUP){
		mHolded = false;
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
			enoughRadius =GetRadius() > 4;
			break;
		case Tool::COLOR_PICKER:
			enoughRadius = true;
			break;
		default:
			ErrorPrint("mUsedTool can't have the value "+static_cast<int>(mUsedTool));
			enoughRadius = false;
			break;
	}

	if(enoughRadius || !mHolded){
		SDL_Point mousePosition;
		SDL_GetMouseState(&mousePosition.x, &mousePosition.y);
		SDL_Rect nViewport = {viewport.x + std::max(mDimensions.x, 0), viewport.y + std::max(mDimensions.y, 0), std::min(mDimensions.w, viewport.w), std::min(mDimensions.h, viewport.h)};
		SDL_RenderSetViewport(pRenderer, &nViewport);

		//We find the middle pixel
		SDL_Point pixel = GetPointCell({mousePosition.x-viewport.x-mDimensions.x, mousePosition.y-viewport.y-mDimensions.y}, mResolution);
		
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
		SDL_Point mouseToCanvas = {mousePosition.x-(std::max(mDimensions.x, 0)+viewport.x), mousePosition.y-(std::max(mDimensions.y, 0)+viewport.y)};
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
			default:
				ErrorPrint("mUsedTool can't have the value "+static_cast<int>(mUsedTool));
				break;
		};
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
	squaresViewport = {viewport.x + std::max(dimensions.x, 0), viewport.y + std::max(dimensions.y, 0), std::min(dimensions.w, viewport.w) + xOffset, std::min(dimensions.h, viewport.h) + yOffset};

	const int MAX_X_SQUARES = 10, MAX_Y_SQUARES = ceil(MAX_X_SQUARES*dimensions.h*1.0f/dimensions.w), SQUARE_SIZE = ceil(dimensions.w*1.0f/MAX_X_SQUARES);

	lightGreySquares.clear(); darkGreySquares.clear();
	//This works as long as MAX_X_SQUARES is even (would also work if MAX_Y_SQUARES is even)
	lightGreySquares.reserve(MAX_X_SQUARES * MAX_Y_SQUARES / 2); darkGreySquares.reserve(MAX_X_SQUARES * MAX_Y_SQUARES / 2);
	SDL_Color grey[2] = {SDL_Color{205, 205, 205}, SDL_Color{155, 155, 155}};

	int index = 0;
	for(int squareX = 0; squareX < MAX_X_SQUARES; squareX++){
		SDL_Rect squareRect = {squareX * SQUARE_SIZE + xOffset, yOffset, SQUARE_SIZE, SQUARE_SIZE};
		for(int squareY = 0; squareY < MAX_Y_SQUARES; squareY++){
			if(index == 1){
				index = 0;
				darkGreySquares.push_back(squareRect);
			} else {
				index = 1;
				lightGreySquares.push_back(squareRect);
			}

			squareRect.y += SQUARE_SIZE;
		}
		index++; //Causes the greys to alternate
	}
}