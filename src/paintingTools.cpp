#include "paintingTools.hpp"
#include "renderLib.hpp"
#include "logger.hpp"
#include <iomanip>
#include <functional>
#include <algorithm>
#include <future>

/*POSITION PICKER BUTTON METHODS:

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
}*/

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
	if(mPencilType != PencilType::HARD) UpdateCirclePixels();
}

float Pencil::GetHardness(){
	return mHardness;
}

void Pencil::SetAlphaCalculation(AlphaCalculation nAlphaCalculation){
	mAlphaCalculation = nAlphaCalculation;
	//If the pencil is hard, then alpha calculation mode has no effect on the pixels alphas
	if(mPencilType != PencilType::HARD) UpdateCirclePixels();
}

void Pencil::SetPencilType(PencilType nPencilType){
	mPencilType = nPencilType;
	UpdateCirclePixels();
}

void Pencil::ApplyOn(const std::span<SDL_Point> circleCenters, SDL_Color circleColor, SDL_Surface *pSurfaceToModify, SDL_Rect *pTotalUsedArea){
	int smallestX = INT_MAX, biggestX = INT_MIN, smallestY = INT_MAX, biggestY = INT_MIN;
	bool changeWasApplied = false;

	//std::unique_ptr<SDL_Renderer, PointerDeleter> pRenderer(SDL_CreateSoftwareRenderer(pSurfaceToModify));

	for(const auto &center : circleCenters){
		SDL_Rect drawArea = {center.x - mRadius, center.y - mRadius, 2*mRadius+1, 2*mRadius+1};
		
		// Create a renderer
		SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(pSurfaceToModify);

		// Create a texture from the circle surface
		SDL_Texture* circleTexture = SDL_CreateTextureFromSurface(renderer, mpCircleSurface.get());

		// Set the blend mode for the texture
		SDL_SetTextureBlendMode(circleTexture, SDL_BLENDMODE_BLEND);

		// Set the color modulation for the texture (if needed)
		SDL_SetTextureColorMod(circleTexture, circleColor.r, circleColor.g, circleColor.b);

		// Set the alpha modulation for the texture (if needed)
		SDL_SetTextureAlphaMod(circleTexture, circleColor.a);
		
		// Render the circle texture with blending
		SDL_RenderCopy(renderer, circleTexture, nullptr, &drawArea);

		// Update the screen
		SDL_RenderPresent(renderer);

		// Clean up resources
		SDL_DestroyTexture(circleTexture);
		SDL_DestroyRenderer(renderer);

		/*
		SDL_Rect givenArea = {0, 0, pSurfaceToModify->w, pSurfaceToModify->h},  usedArea = {0,0,0,0};
		if(SDL_IntersectRect(&givenArea, &drawArea, &usedArea) == SDL_FALSE) continue;
		SDL_Point circleOffset = {usedArea.x-drawArea.x, usedArea.y-drawArea.y};
		
		float appliedAlpha = (circleColor.a/255.0f), apR = circleColor.r/255.0f, apG = circleColor.g/255.0f, apB = circleColor.b/255.0f;
		*/


		/*for(int y = 0; y < usedArea.h; ++y){
			for(int x = 0; x < usedArea.w; ++x){
				Uint32 *baseColor = UnsafeGetPixelFromSurface<Uint32>({x+usedArea.x, y+usedArea.y}, pSurfaceToModify);
				SDL_Color actualColor = {0, 0, 0, 0};
				SDL_GetRGBA(*baseColor, pSurfaceToModify->format, &actualColor.r, &actualColor.g, &actualColor.b, &actualColor.a);
				Uint8 *appliedColor = UnsafeGetPixelFromSurface<Uint8>({x+circleOffset.x, y+circleOffset.y}, mpCircleSurface.get());
				float bsA = actualColor.a/255.0f, bsR = actualColor.r/255.0f, bsG = actualColor.g/255.0f, bsB = actualColor.b/255.0f;
				float apA = appliedAlpha**(appliedColor)/255.0f, resultA = apA+bsA*(1-apA);
				*baseColor = SDL_MapRGBA(pSurfaceToModify->format, SDL_ALPHA_OPAQUE*((apR*apA+bsR*bsA*(1-apA))/resultA), SDL_ALPHA_OPAQUE*((apG*apA+bsG*bsA*(1-apA))/resultA), SDL_ALPHA_OPAQUE*((apB*apA+bsB*bsA*(1-apA))/resultA), SDL_ALPHA_OPAQUE*(resultA));
				*baseColor = SDL_MapRGBA(pSurfaceToModify->format, circleColor.r, circleColor.g, circleColor.b, 255);
			}
		}*/

		//SDL_SetSurfaceColorMod(mpCircleSurface.get(), circleColor.r, circleColor.g, circleColor.b);
		//SDL_SetSurfaceAlphaMod(mpCircleSurface.get(), circleColor.a);
		//SDL_SetSurfaceBlendMode(mpCircleSurface.get(), SDL_BLENDMODE_BLEND);
		//SDL_BlitSurface(mpCircleSurface.get(), nullptr, pSurfaceToModify, &drawArea);
        
		changeWasApplied = true;
		if(smallestX > center.x) smallestX = center.x;
		if(biggestX < center.x) biggestX = center.x;
		if(smallestY > center.y) smallestY = center.y;
		if(biggestY < center.y) biggestY = center.y;
	}
	
	SDL_SetSurfaceColorMod(mpCircleSurface.get(), 255, 255, 255);
	SDL_SetSurfaceAlphaMod(mpCircleSurface.get(), SDL_ALPHA_OPAQUE);

	if(pTotalUsedArea != nullptr){
		if(changeWasApplied){
			SDL_Rect actualArea = {smallestX - mRadius, smallestY - mRadius, (biggestX-smallestX) + 2*mRadius+1, (biggestY-smallestY) + 2*mRadius+1};
			SDL_Rect surfaceArea = {0, 0, pSurfaceToModify->w, pSurfaceToModify->h};
			SDL_IntersectRect(&actualArea, &surfaceArea, pTotalUsedArea);
		} else {
			*pTotalUsedArea = {0, 0, 0, 0};
		}
	}
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
		rects[i].x += center.x-((int)roundf(mRectsResolution))/2;
		rects[i].y += center.y-((int)roundf(mRectsResolution))/2;
	}
	
	SDL_RenderFillRects(pRenderer, rects, mPreviewRects.size());

	delete[] rects;
}

void Pencil::UpdateCirclePixels(){
	mpCircleSurface.reset(SDL_CreateRGBSurfaceWithFormat(0, 2*mRadius+1, 2*mRadius+1, 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888));
	SDL_Rect surfaceRect {0, 0, mpCircleSurface->w, mpCircleSurface->h};
	SDL_FillRect(mpCircleSurface.get(), &surfaceRect, 0);
	SDL_SetSurfaceBlendMode(mpCircleSurface.get(), SDL_BLENDMODE_BLEND);

	//(mRadius, mRadius) is the middle pixel for the surface
	const SDL_Point center = {mRadius, mRadius};
	
	//This is made in order not to write multiple times '.get()'. Not so much due to optimizing, but rather to avoid repetition
	SDL_Surface *pCircleSurface = mpCircleSurface.get();

	//Based on "Midpoint circle algorithm - Jesko's Method", in such a way so that there are no repeating points
    int x = 0, y = mRadius;
	int t1 = mRadius/16, t2;

	auto circleCicle = [&](){
		x++;
		t1 += x;
		t2 = t1 - y;
		if(t2 >= 0){
			int xMinus = x-1;
			if(xMinus!=y){
				FillHorizontalLine(center.y+y, center.x-xMinus, center.x+xMinus, center, pCircleSurface);
				FillHorizontalLine(center.y-y, center.x-xMinus, center.x+xMinus, center, pCircleSurface);
			}
			t1 = t2;
			y--;
		} 
	};

	FillHorizontalLine(center.y, center.x-y, center.x+y, center, pCircleSurface);
	circleCicle();
	
	while(y >= x){
		FillHorizontalLine(center.y+x, center.x-y, center.x+y, center, pCircleSurface);
		FillHorizontalLine(center.y-x, center.x-y, center.x+y, center, pCircleSurface);

		circleCicle();
	}
}

Uint8 Pencil::GetPixelAlpha(const SDL_Point &center, const SDL_Point &pixel){
	if(mPencilType ==  PencilType::HARD){
		return SDL_ALPHA_OPAQUE;
	
	} else if(mPencilType ==  PencilType::SOFT){
		//TODO: modify hardness system (mHardness = 1 doesn't make all the pixels fully opaque (and (mRadius+1)*mHardness is a bit of an overkill))
		float centerDistance = sqrt(pow(pixel.x-center.x, 2) + pow(pixel.y-center.y, 2));
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

		return alpha;
	}

	return -1;
}

void Pencil::FillHorizontalLine(int y, int minX, int maxX, const SDL_Point &circleCenter, SDL_Surface *pSurface){
	for(int x = minX; x <= maxX; ++x){
		SDL_Point pos {x, y};
		SDL_Rect surf {0, 0, pSurface->w, pSurface->h};
		if(SDL_PointInRect(&pos, &surf) == SDL_TRUE)*UnsafeGetPixelFromSurface<Uint32>({x, y}, pSurface) = SDL_MapRGBA(pSurface->format, 255, 255, 255, GetPixelAlpha(circleCenter, {x, y}));
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
	const std::function<float(float)> ROUND = roundf;

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

	int x = 0, y = radius;
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

//MUTABLE TEXTURE METHODS:

MutableTexture::MutableTexture(SDL_Renderer *pRenderer, int width, int height, SDL_Color fillColor){
	mSurfaces.resize(1);
	mTextures.resize(1);
	mSurfaces[mSelectedLayer].reset(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888));
    mTextures[mSelectedLayer].reset(SDL_CreateTexture(pRenderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, width, height));

	Clear(fillColor);
}

MutableTexture::MutableTexture(SDL_Renderer *pRenderer, const char *pImage){
	SDL_Surface *loaded = IMG_Load(pImage);
	
	mSurfaces.resize(1);
	mTextures.resize(1);
	mSurfaces[mSelectedLayer].reset(SDL_ConvertSurfaceFormat(loaded, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888, 0));
    mTextures[mSelectedLayer].reset(SDL_CreateTexture(pRenderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, loaded->w, loaded->h));
	
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
	for(int i = 0; i < mSurfaces.size(); ++i){
		SDL_Color auxiliar;
		SDL_GetRGBA(*UnsafeGetPixel(pixel, i), mSurfaces[i]->format, &auxiliar.r, &auxiliar.g, &auxiliar.b, &auxiliar.a);
		ApplyColorToColor(pixelColor, auxiliar);
	}
	return pixelColor;
}

void MutableTexture::Clear(const SDL_Color &clearColor){
	//Currently only clears the current layer
	SDL_Rect surfaceRect {0, 0, mSurfaces[mSelectedLayer]->w, mSurfaces[mSelectedLayer]->h};
	SDL_FillRect(mSurfaces[mSelectedLayer].get(), &surfaceRect, SDL_MapRGBA(mSurfaces[mSelectedLayer]->format, clearColor.r, clearColor.g, clearColor.b, clearColor.a));

	UpdateWholeTexture();
}

void MutableTexture::ApplyColorToColor(SDL_Color &baseColor, const SDL_Color &appliedColor){
	/*
	The following may be added if shown to improve speed in cases
	if(appliedColor.a == 255){
		baseColor.a = appliedColor.a;
		baseColor.r = appliedColor.r;
		baseColor.g = appliedColor.g;
		baseColor.b = appliedColor.b;
		return;
	}
	*/

	float bsA = baseColor.a/255.0f, bsR = baseColor.r/255.0f, bsG = baseColor.g/255.0f, bsB = baseColor.b/255.0f;
	float apA = appliedColor.a/255.0f, apR = appliedColor.r/255.0f, apG = appliedColor.g/255.0f, apB = appliedColor.b/255.0f;
	float resultA = apA+bsA*(1-apA);
	baseColor.a = SDL_ALPHA_OPAQUE*(resultA);
	baseColor.r = SDL_ALPHA_OPAQUE*((apR*apA+bsR*bsA*(1-apA))/resultA);
	baseColor.g = SDL_ALPHA_OPAQUE*((apG*apA+bsG*bsA*(1-apA))/resultA);
	baseColor.b = SDL_ALPHA_OPAQUE*((apB*apA+bsB*bsA*(1-apA))/resultA);

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
	*UnsafeGetPixel(pixel) = SDL_MapRGBA(mSurfaces[mSelectedLayer]->format, color.r, color.g, color.b, color.a);

	mChangedPixels.push_back(pixel);
}

void MutableTexture::SetPixelsUnsafe(std::span<SDL_Point> pixels, const SDL_Color &color){
	for(const auto &pixel : pixels){
		*UnsafeGetPixel(pixel) = SDL_MapRGBA(mSurfaces[mSelectedLayer]->format, color.r, color.g, color.b, color.a);
	}

	mChangedPixels.insert(mChangedPixels.end(), pixels.begin(), pixels.end());
}

SDL_Surface *MutableTexture::GetCurrentSurface(){
	return mSurfaces[mSelectedLayer].get();
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

	SDL_LockTextureToSurface(mTextures[mSelectedLayer].get(), &rect, &texturesSurface);
	
	SDL_SetSurfaceBlendMode(mSurfaces[mSelectedLayer].get(), SDL_BLENDMODE_NONE);
	SDL_BlitSurface(mSurfaces[mSelectedLayer].get(), &rect, texturesSurface, nullptr);
	SDL_SetSurfaceBlendMode(mSurfaces[mSelectedLayer].get(), SDL_BLENDMODE_BLEND);
	
	SDL_UnlockTexture(mTextures[mSelectedLayer].get());
}

void MutableTexture::AddLayer(SDL_Renderer *pRenderer){
	mSurfaces.emplace(mSurfaces.begin()+mSelectedLayer+1, SDL_CreateRGBSurfaceWithFormat(0, GetWidth(), GetHeight(), 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888));
    mTextures.emplace(mTextures.begin()+mSelectedLayer+1, SDL_CreateTexture(pRenderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, GetWidth(), GetHeight()));
	mSelectedLayer++;

	SDL_SetSurfaceBlendMode(mSurfaces[mSelectedLayer].get(), SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(mTextures[mSelectedLayer].get(), SDL_BLENDMODE_BLEND);
	
	//Currently all new layers are created with no colors
	Clear({255, 255, 255, SDL_ALPHA_TRANSPARENT});
}

void MutableTexture::DeleteCurrentLayer(){
	if(mSurfaces.size() == 1){
		DebugPrint("Can't delete current layer, as it is the last one left");
		return;
	}

	//Better be careful than sorry
	mSelectedLayer = std::clamp(mSelectedLayer, 0, (int)mSurfaces.size()-1);

	mSurfaces.erase(mSurfaces.begin() + mSelectedLayer);
	mTextures.erase(mTextures.begin() + mSelectedLayer);
	if(mSelectedLayer != 0) mSelectedLayer--;
}

void MutableTexture::SetLayer(int nLayer){
	nLayer = std::clamp(nLayer, 0, (int)mSurfaces.size()-1);
	mSelectedLayer = nLayer;
}

int MutableTexture::GetLayer(){
	return mSelectedLayer;
}

int MutableTexture::GetTotalLayers(){
	return mSurfaces.size();
}

void MutableTexture::DrawIntoRenderer(SDL_Renderer *pRenderer, const SDL_Rect &dimensions){
	for(const auto& texture : mTextures) SDL_RenderCopy(pRenderer, texture.get(), nullptr, &dimensions);
}

bool MutableTexture::Save(const char *pSavePath){
	std::unique_ptr<SDL_Surface, PointerDeleter> saveSurface(SDL_CreateRGBSurfaceWithFormat(0, GetWidth(), GetHeight(), 32, SDL_PixelFormatEnum::SDL_PIXELFORMAT_ARGB8888));

	for(const auto& surface : mSurfaces){
		SDL_BlitSurface(surface.get(), nullptr, saveSurface.get(), nullptr);
	}

	if(IMG_SavePNG(saveSurface.get(), pSavePath)){
		ErrorPrint("Couldn't save image in file "+std::string(pSavePath));
		return true;
	}
	return false;
}

int MutableTexture::GetWidth(){
	return mSurfaces[0]->w;
}

int MutableTexture::GetHeight(){
	return mSurfaces[0]->h;
}

void MutableTexture::UpdateWholeTexture(){
	SDL_Surface *texturesSurface;

	for(int i = 0; i < mSurfaces.size(); ++i){
		SDL_LockTextureToSurface(mTextures[i].get(), nullptr, &texturesSurface);
		
		//Makes sure that the texture gets updated with the exact info (no blending involved)
		SDL_SetSurfaceBlendMode(mSurfaces[i].get(), SDL_BLENDMODE_NONE);
		SDL_BlitSurface(mSurfaces[i].get(), nullptr, texturesSurface, nullptr);
		SDL_SetSurfaceBlendMode(mSurfaces[i].get(), SDL_BLENDMODE_BLEND);

		SDL_UnlockTexture(mTextures[i].get());

		SDL_SetTextureBlendMode(mTextures[i].get(), SDL_BLENDMODE_BLEND);
	}
	
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
	UpdateRealPosition();
}

Canvas::Canvas(SDL_Renderer *pRenderer, const char *pLoadFile) : mpImage(new MutableTexture(pRenderer, pLoadFile)){
	mDimensions = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};
	UpdateRealPosition();
}

Canvas::~Canvas(){
	if(saveOnDestroy) Save();
}

void Canvas::Resize(SDL_Renderer *pRenderer, int nWidth, int nHeight){
	mpImage.reset(new MutableTexture(pRenderer, nWidth, nHeight));
	mDimensions = {0, 0, nWidth, nHeight};
	UpdateRealPosition();
}

void Canvas::OpenFile(SDL_Renderer *pRenderer, const char *pLoadFile){
	mpImage.reset(new MutableTexture(pRenderer, pLoadFile));
	mDimensions = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};
	UpdateRealPosition();
}

SDL_Color Canvas::GetColor(){
	return mDrawColor;
}

void Canvas::SetColor(SDL_Color nDrawColor){
	mDrawColor = nDrawColor;
}

void Canvas::DrawPixel(SDL_Point localPixel){
	SDL_Rect usedArea{0, 0, 0, 0}, imageRect = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};

	std::vector<SDL_Point> pixels{localPixel};
	pencil.ApplyOn(pixels, mDrawColor, mpImage->GetCurrentSurface(), &usedArea);
	mpImage->UpdateTexture(usedArea);

	mLastMousePixel = localPixel;
}

void Canvas::DrawPixels(const std::vector<SDL_Point> &localPixels){
	SDL_Rect usedArea{0, 0, 0, 0}, imageRect = {0, 0, mpImage->GetWidth(), mpImage->GetHeight()};
	
	std::vector<SDL_Point> pixels(localPixels);
	pencil.ApplyOn(pixels, mDrawColor, mpImage->GetCurrentSurface(), &usedArea);
	mpImage->UpdateTexture(usedArea);

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
	UpdateRealPosition();
}

void Canvas::SetResolution(float nResolution){
	//if(nResolution < M_MIN_RESOLUTION || nResolution > M_MAX_RESOLUTION) return;
	mResolution = std::clamp(nResolution, M_MIN_RESOLUTION, M_MAX_RESOLUTION);
	pencil.SetResolution(mResolution);

	int nWidth = mpImage->GetWidth()*mResolution, nHeight = mpImage->GetHeight()*mResolution;

	mRealPosition.x = viewport.w/2 - (viewport.w/2-(mRealPosition.x)) * nWidth * 1.0f / mDimensions.w;
	mRealPosition.y = viewport.h/2 - (viewport.h/2-(mRealPosition.y)) * nHeight * 1.0f / mDimensions.h;
	mDimensions.x = (int)(mRealPosition.x);
	mDimensions.y = (int)(mRealPosition.y);
	mDimensions.w = nWidth;
	mDimensions.h = nHeight;
}

void Canvas::HandleEvent(SDL_Event *event){
	if(event->type == SDL_MOUSEBUTTONDOWN){
        //Check the mouse is in the viewport
        SDL_Point mousePos = {event->button.x, event->button.y};
		if(!SDL_PointInRect(&mousePos, &viewport)) return;

		SDL_Point pixel = GetPointCell({mousePos.x-(mDimensions.x+viewport.x), mousePos.y-(mDimensions.y+viewport.y)}, mResolution);

		switch(usedTool){
			case Tool::DRAW_TOOL: 
				DrawPixel(pixel);
				break;

			case Tool::ERASE_TOOL:{
				//TODO: add Tool base class and system
				/*std::vector<SDL_Point> pixels = pencil.GetAffectedPixels(pixel, nullptr);
				mpImage->SetPixels(pixels, {0, 0, 0, SDL_ALPHA_TRANSPARENT});
				mLastMousePixel = pixel;*/
				break;
			}

			default:
				ErrorPrint("usedTool can't have the value "+static_cast<int>(usedTool));
				break;
		}

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
		if(ArePointsEqual(mLastMousePixel, pixel)) return;
		
		//If the last pixel where the mouse was registered and the current pixel are separated by more than 1 unit in any axis, we also set the pixels in between
		if(std::abs(pixel.x-mLastMousePixel.x) > 1 || std::abs(pixel.y-mLastMousePixel.y) > 1){
			
			switch(usedTool){
				case Tool::DRAW_TOOL: 
					DrawPixels(GetPointsInSegment(mLastMousePixel, pixel));
					break;

				case Tool::ERASE_TOOL:{
					//TODO: add Tool base class and system
					/*std::vector<SDL_Point> mainPixels = GetPointsInSegment(mLastMousePixel, pixel);
					for(auto nPixel : mainPixels){
						std::vector<SDL_Point> pixels = pencil.GetAffectedPixels(nPixel, nullptr);
						mpImage->SetPixels(pixels, {0, 0, 0, SDL_ALPHA_TRANSPARENT});
					}
					mLastMousePixel = mainPixels.back();*/
					break;
				}

				default:
					ErrorPrint("usedTool can't have the value "+static_cast<int>(usedTool));
					break;
			}
		} else {
			switch(usedTool){
				case Tool::DRAW_TOOL: 
					DrawPixel(pixel);
					break;

				case Tool::ERASE_TOOL:{
					//TODO: add Tool base class and system
					/*std::vector<SDL_Point> pixels = pencil.GetAffectedPixels(pixel, nullptr);
					mpImage->SetPixels(pixels, {0, 0, 0, SDL_ALPHA_TRANSPARENT});
					mLastMousePixel = pixel;*/
					break;
				}

				default:
					ErrorPrint("usedTool can't have the value "+static_cast<int>(usedTool));
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
		//pencil.SetHardness(pencil.GetHardness() + event->wheel.y*0.1f);
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
	}
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

	SDL_Rect intersectionRect = {mDimensions.x + viewport.x, mDimensions.y + viewport.y, std::min(mDimensions.w, viewport.w), std::min(mDimensions.h, viewport.h)};
	SDL_RenderSetViewport(pRenderer, &intersectionRect);

    const int MAX_X_SQUARES = 10, MAX_Y_SQUARES = ceil(MAX_X_SQUARES*mDimensions.h*1.0f/mDimensions.w), SQUARE_SIZE = ceil(mDimensions.w*1.0f/MAX_X_SQUARES);

	for(int squareX = 0; squareX < MAX_X_SQUARES; squareX++){
		SDL_Rect squareRect = {squareX * SQUARE_SIZE, 0, SQUARE_SIZE, SQUARE_SIZE};
		for(int squareY = 0; squareY < MAX_Y_SQUARES; squareY++){
			Uint8 squareColor = ((squareX+squareY) % 2 == 0)?205:155;

			SDL_SetRenderDrawColor(pRenderer, squareColor, squareColor, squareColor, SDL_ALPHA_OPAQUE);
			SDL_RenderFillRect(pRenderer, &squareRect);

			squareRect.y += SQUARE_SIZE;
		}
	}

	SDL_RenderSetViewport(pRenderer, &viewport);
	mpImage->DrawIntoRenderer(pRenderer, mDimensions);

	if(!mHolded || pencil.GetRadius() > 4){
		SDL_Point mousePosition;
		SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

		SDL_RenderSetViewport(pRenderer, &intersectionRect);

		//We find the middle pixel
		SDL_Point pixel = GetPointCell({mousePosition.x-mDimensions.x, mousePosition.y-mDimensions.y}, mResolution);
		
		SDL_Color previewColor = pencilDisplayMainColor;
		bool validColor = true;
		SDL_Color pixelColor = mpImage->GetPixelColor(pixel, &validColor);
		if(validColor && ((int)pixelColor.r + (int)pixelColor.g + (int)pixelColor.b)*(pixelColor.a/255.0f) <= 127.0f*3){
			previewColor = pencilDisplayAlternateColor;
		}

        /*
        We would perform these operations in order to then send pixel to Pencil::DrawPreview. This has been discarded as it wouldn't result in a pixel perfect preview 	
		pixel.x *= mResolution; pixel.y *= mResolution;
		pixel.x -= intersectionRect.x-mDimensions.x; pixel.y -= intersectionRect.y-mDimensions.y;
		*/

		//We find the middle pixel
		SDL_Point mouseToCanvas = {mousePosition.x-(mDimensions.x+viewport.x), mousePosition.y-(mDimensions.y+viewport.y)};
		pencil.DrawPreview(mouseToCanvas, pRenderer, previewColor);
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