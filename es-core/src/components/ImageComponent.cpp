#include <string>
#include "components/ImageComponent.h"

#include "resources/TextureResource.h"
#include "Log.h"
#include "renderers/Renderer.h"
#include "Settings.h"
#include "ThemeData.h"

#include "resources/TextureData.h"
#include "utils/FileSystemUtil.h"

Vector2i ImageComponent::getTextureSize() const
{
	if(mTexture)
		return mTexture->getSize();
	else
		return Vector2i::Zero();
}

Vector2f ImageComponent::getSize() const
{
	// AmberELEC: with <padding>, a max-size image reports its padded size on the constrained axis
	if (mTargetIsMax && mPadding != Vector4f::Zero())
	{
		Vector2f targetSize = mTargetSize - Vector2f(mPadding.x() + mPadding.z(), mPadding.y() + mPadding.w());

		if (mSize.x() == targetSize.x())
			return Vector2f(mSize.x() + mPadding.x() + mPadding.z(), mSize.y());
		else if (mSize.y() == targetSize.y())
			return Vector2f(mSize.x(), mSize.y() + mPadding.y() + mPadding.w());
	}

	return GuiComponent::getSize() * (mBottomRightCrop - mTopLeftCrop);
}

ImageComponent::ImageComponent(Window* window, bool forceLoad, bool dynamic) : GuiComponent(window),
	mTargetIsMax(false), mTargetIsMin(false), mFlipX(false), mFlipY(false), mTargetSize(0, 0), mColorShift(0xFFFFFFFF), mColorShiftEnd(0xFFFFFFFF),
	mForceLoad(forceLoad), mDynamic(dynamic), mFadeOpacity(0), mFading(false), mRotateByTargetSize(false),
	mTopLeftCrop(0.0f, 0.0f), mBottomRightCrop(1.0f, 1.0f), mMirror(0.0f, 0.0f), mAllowAsync(false)
{
	mLinear = false;
	mHorizontalAlignment = ALIGN_CENTER;
	mVerticalAlignment = ALIGN_CENTER;
	mReflectOnBorders = false;
	mLoadingTexture = nullptr;
	mAllowFading = true;
	mRoundCorners = 0.0f;
	mShowing = false;
	mPlaylistTimer = 0;
}

ImageComponent::~ImageComponent()
{
}

void ImageComponent::resize()
{
	if (!mTexture)
		return;

	const Vector2f textureSize = mTexture->getSourceImageSize();
	if (textureSize == Vector2f::Zero())
		return;

	// AmberELEC <padding>: fit inside the target size minus the padding
	const Vector2f targetSize = mTargetSize - Vector2f(mPadding.x() + mPadding.z(), mPadding.y() + mPadding.w());

	int ox = mSize.x();
	int oy = mSize.y();

	if (mTexture->isTiled())
	{
		uncrop();
		mSize = targetSize;
	}
	else
	{
		// SVG rasterization is determined by height (see SVGResource.cpp), and rasterization is done in terms of pixels
		// if rounding is off enough in the rasterization step (for images with extreme aspect ratios), it can cause cutoff when the aspect ratio breaks
		// so, we always make sure the resultant height is an integer to make sure cutoff doesn't happen, and scale width from that 
		// (you'll see this scattered throughout the function)
		// this is probably not the best way, so if you're familiar with this problem and have a better solution, please make a pull request!

		if(mTargetIsMax)
		{
			uncrop();
			mSize = textureSize;

			Vector2f resizeScale((targetSize.x() / mSize.x()), (targetSize.y() / mSize.y()));

			if(resizeScale.x() < resizeScale.y())
			{
				mSize[0] *= resizeScale.x(); // this will be targetSize.x(). We can't exceed it, nor be lower than it.
				// we need to make sure we're not creating an image larger than max size
				mSize[1] = Math::min(Math::round(mSize[1] *= resizeScale.x()), targetSize.y());
			}else{
				mSize[1] = Math::round(mSize[1] * resizeScale.y()); // this will be targetSize.y(). We can't exceed it.
				
				// for SVG rasterization, always calculate width from rounded height (see comment above)
				// we need to make sure we're not creating an image larger than max size
				mSize[0] = Math::min((mSize[1] / textureSize.y()) * textureSize.x(), targetSize.x());
			}

		}else if(mTargetIsMin)
		{
			mSize = textureSize;

			Vector2f resizeScale((targetSize.x() / mSize.x()), (targetSize.y() / mSize.y()));

			if(resizeScale.x() > resizeScale.y())
			{
				mSize[0] *= resizeScale.x();
				mSize[1] *= resizeScale.x();

				float cropPercent = (mSize.y() - targetSize.y()) / (mSize.y() * 2);
				crop(0, cropPercent, 0, cropPercent);
			}else{
				mSize[0] *= resizeScale.y();
				mSize[1] *= resizeScale.y();

				float cropPercent = (mSize.x() - targetSize.x()) / (mSize.x() * 2);
				crop(cropPercent, 0, cropPercent, 0);
			}

			// for SVG rasterization, always calculate width from rounded height (see comment above)
			// we need to make sure we're not creating an image smaller than min size
			mSize[1] = Math::max(Math::round(mSize[1]), targetSize.y());
			mSize[0] = Math::max((mSize[1] / textureSize.y()) * textureSize.x(), targetSize.x());

		}
		else
		{
			uncrop();
			// if both components are set, we just stretch
			// if no components are set, we don't resize at all
			mSize = targetSize == Vector2f::Zero() ? textureSize : targetSize;

			// if only one component is set, we resize in a way that maintains aspect ratio
			// for SVG rasterization, we always calculate width from rounded height (see comment above)
			if(!targetSize.x() && targetSize.y())
			{
				mSize[1] = Math::round(targetSize.y());
				mSize[0] = (mSize.y() / textureSize.y()) * textureSize.x();
			}else if(targetSize.x() && !targetSize.y())
			{
				mSize[1] = Math::round((targetSize.x() / textureSize.x()) * textureSize.y());
				mSize[0] = (mSize.y() / textureSize.y()) * textureSize.x();
			}
		}
	}

	mSize[0] = Math::round(mSize.x());
	mSize[1] = Math::round(mSize.y());

	mTexture->rasterizeAt(mSize.x(), mSize.y());

	onSizeChanged();
}

void ImageComponent::onSizeChanged()
{
	updateVertices();
}

void ImageComponent::setSaturation(float saturation)
{
	if (mSaturation == saturation)
		return;

	mSaturation = saturation;

	// reload the current image with the new saturation
	if (!mPath.empty())
	{
		std::string path = mPath;
		mPath = "";
		setImage(path, mTexture != nullptr && mTexture->isTiled(), getMaxSizeInfo());
	}
}

void ImageComponent::setDefaultImage(std::string path)
{
	mDefaultPath = path;
}

void ImageComponent::setImage(std::string path, bool tile, MaxSizeInfo maxSize)
{
	std::string canonicalPath = Utils::FileSystem::getCanonicalPath(path);
	if (mPath == canonicalPath)
		return;
	
	mPath = canonicalPath;

	// If the previous image is in the async queue, remove it
	TextureResource::cancelAsync(mLoadingTexture);
	TextureResource::cancelAsync(mTexture);
	mLoadingTexture.reset();

	if (mPath.empty() || !ResourceManager::getInstance()->fileExists(mPath))
	{
		if (mDefaultPath.empty() || !ResourceManager::getInstance()->fileExists(mDefaultPath))
			mTexture.reset();
		else
			mTexture = TextureResource::get(mDefaultPath, tile, mLinear, mForceLoad, mDynamic, true, maxSize, mSaturation);
	}
	else
	{
		std::shared_ptr<TextureResource> texture = TextureResource::get(mPath, tile, mLinear, mForceLoad, mDynamic, true, maxSize, mSaturation);

		if (!mForceLoad && mDynamic && !mAllowFading && texture != nullptr && !texture->isLoaded())
			mLoadingTexture = texture;
		else
			mTexture = texture;
	}

	if (mLoadingTexture == nullptr)
		resize();
}

void ImageComponent::setImage(const char* path, size_t length, bool tile)
{
	mTexture.reset();
	mTexture = TextureResource::get("", tile);
	mTexture->initFromMemory(path, length);
	
	resize();
}

void ImageComponent::setImage(const std::shared_ptr<TextureResource>& texture)
{
	mTexture = texture;
	resize();
}

void ImageComponent::setResize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && !mTargetIsMax && !mTargetIsMin && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mTargetIsMax = false;
	mTargetIsMin = false;
	resize();
}

void ImageComponent::setMaxSize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && mTargetIsMax && !mTargetIsMin && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mTargetIsMax = true;
	mTargetIsMin = false;
	resize();
}

void ImageComponent::setMinSize(float width, float height)
{
	if (mSize.x() != 0 && mSize.y() != 0 && mTargetIsMin && !mTargetIsMax && mTargetSize.x() == width && mTargetSize.y() == height)
		return;

	mTargetSize = Vector2f(width, height);
	mTargetIsMax = false;
	mTargetIsMin = true;
	resize();
}

Vector2f ImageComponent::getRotationSize() const
{
	return mRotateByTargetSize ? mTargetSize : mSize;
}

void ImageComponent::setRotateByTargetSize(bool rotate)
{
	mRotateByTargetSize = rotate;
}

void ImageComponent::cropLeft(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mTopLeftCrop.x() = percent;
}

void ImageComponent::cropTop(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mTopLeftCrop.y() = percent;
}

void ImageComponent::cropRight(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mBottomRightCrop.x() = 1.0f - percent;
}

void ImageComponent::cropBot(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mBottomRightCrop.y() = 1.0f - percent;
}

void ImageComponent::crop(float left, float top, float right, float bot)
{
	cropLeft(left);
	cropTop(top);
	cropRight(right);
	cropBot(bot);
}

void ImageComponent::uncrop()
{
	crop(0, 0, 0, 0);
}

void ImageComponent::setFlipX(bool flip)
{
	mFlipX = flip;
	updateVertices();
}

void ImageComponent::setFlipY(bool flip)
{
	mFlipY = flip;
	updateVertices();
}

void ImageComponent::setColorShift(unsigned int color)
{
	mColorShift = color;
	mColorShiftEnd = color;
	// Grab the opacity from the color shift because we may need to apply it if
	// fading textures in
	//mOpacity = color & 0xff;	
}

void ImageComponent::setColorShiftEnd(unsigned int color)
{
	mColorShiftEnd = color;
}

void ImageComponent::setColorGradientHorizontal(bool horizontal)
{
	mColorGradientHorizontal = horizontal;
}

void ImageComponent::setOpacity(unsigned char opacity)
{
	mOpacity = opacity;
	//mColorShift = (mColorShift >> 8 << 8) | mOpacity;	
	//mColorShiftEnd = (mColorShiftEnd >> 8 << 8) | mOpacity;
}

void ImageComponent::updateVertices()
{
	if (!mTexture)
		return;

	// we go through this mess to make sure everything is properly rounded
	// if we just round vertices at the end, edge cases occur near sizes of 0.5
	Vector2f           topLeft     = { mSize * mTopLeftCrop };
	Vector2f           bottomRight = { mSize * mBottomRightCrop };

	// AmberELEC <padding>: the image sits inside the padded area
	if (mPadding != Vector4f::Zero())
	{
		Vector2f paddingOffset = Vector2f(mPadding.x() - (mPadding.x() + mPadding.z()) * mOrigin.x(), mPadding.y() - (mPadding.y() + mPadding.w()) * mOrigin.y());
		topLeft = topLeft + paddingOffset;
		bottomRight = bottomRight + paddingOffset;
	}

	const float        px          = mTexture->isTiled() ? mSize.x() / getTextureSize().x() : 1.0f;
	const float        py          = mTexture->isTiled() ? mSize.y() / getTextureSize().y() : 1.0f;
	const unsigned int color       = Renderer::convertColor(mColorShift);
	const unsigned int colorEnd    = Renderer::convertColor(mColorShiftEnd);

	mVertices[0] = { { topLeft.x(),     topLeft.y()     },
		{ mTopLeftCrop.x(),          py - mTopLeftCrop.y()     }, color };

	mVertices[1] = { { topLeft.x(),     bottomRight.y() },
		{ mTopLeftCrop.x(),          1.0f - mBottomRightCrop.y() }, mColorGradientHorizontal ? colorEnd : color };

	mVertices[2] = { { bottomRight.x(), topLeft.y()	},
		{ mBottomRightCrop.x() * px, py - mTopLeftCrop.y()     }, mColorGradientHorizontal ? color : colorEnd };

	mVertices[3] = { { bottomRight.x(), bottomRight.y() },
	{ mBottomRightCrop.x() * px, 1.0f - mBottomRightCrop.y() }, color };

	// round vertices
	for(int i = 0; i < 4; ++i)
		mVertices[i].pos.round();

	if(mFlipX)
	{
		for (int i = 0; i < 4; i++)
			mVertices[i].tex[0] = px - mVertices[i].tex[0];
	}
	if (mFlipY)
	{
		for (int i = 0; i < 4; i++)
			mVertices[i].tex[1] = py - mVertices[i].tex[1];
	}
}

void ImageComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	if (mLoadingTexture != nullptr && mLoadingTexture->isLoaded())
	{
		mTexture = mLoadingTexture;
		mLoadingTexture.reset();
		resize();
	}

	Transform4x4f trans = parentTrans * getTransform();
	
	// Don't use soft clip if rotation applied : let renderer do the work
	if (mRotation == 0 && !Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;
		
	Renderer::setMatrix(trans);

	if (mTexture && mOpacity > 0)
	{
		Vector2f targetSizePos = (mTargetSize - mSize) * mOrigin * -1;

		if (Settings::getInstance()->getBool("DebugImage"))
		{
			Renderer::drawRect(targetSizePos.x(), targetSizePos.y(), mTargetSize.x(), mTargetSize.y(), 0xFF000033);
			Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0x00000033);
		}

		if (!mTexture->bind())
		{
			fadeIn(false);
			return;
		}

		if (mVerticalAlignment == ALIGN_TOP)
			trans.translate(Vector3f(0, targetSizePos.y(), 0.0f));
		else if (mVerticalAlignment == ALIGN_BOTTOM)
			trans.translate(Vector3f(targetSizePos.x(), targetSizePos.y() + mTargetSize.y() - mSize.y(), 0.0f));

		if (mHorizontalAlignment == ALIGN_LEFT)
			trans.translate(Vector3f(targetSizePos.x(), 0, 0.0f));
		else if (mHorizontalAlignment == ALIGN_RIGHT)
			trans.translate(Vector3f(targetSizePos.x() + mTargetSize.x() - mSize.x(), targetSizePos.y(), 0.0f));

		Renderer::setMatrix(trans);

		fadeIn(true);

		float opacity = (mOpacity * (mFading ? mFadeOpacity / 255.0 : 1.0)) / 255.0;

		const unsigned int color = Renderer::convertColor(mColorShift & 0xFFFFFF00 | (unsigned char)((mColorShift & 0xFF) * opacity));
		const unsigned int colorEnd = Renderer::convertColor(mColorShiftEnd & 0xFFFFFF00 | (unsigned char)((mColorShiftEnd & 0xFF) * opacity));

		mVertices[0].col = color;
		mVertices[1].col = mColorGradientHorizontal ? colorEnd : color;
		mVertices[2].col = mColorGradientHorizontal ? color : colorEnd;
		mVertices[3].col = colorEnd;

		if (mRoundCorners > 0)
		{
			float x = 0;
			float y = 0;
			float size_x = mSize.x();
			float size_y = mSize.y();
			
			
			if (mTargetIsMin)
			{
				x = targetSizePos.x();
				y = targetSizePos.y();
				size_x = mTargetSize.x();
				size_y = mTargetSize.y();
			}			

			float radius = Math::max(size_x, size_y) * mRoundCorners;

			Renderer::enableRoundCornerStencil(x, y, size_x, size_y, radius);

			mTexture->bind();
		}			
	
		Renderer::drawTriangleStrips(&mVertices[0], 4);
		
		if (mRoundCorners > 0)
			Renderer::disableStencil();			

		if (mMirror.x() != 0 || mMirror.y() != 0)
		{
			float alpha = ((mColorShift & 0x000000ff)) / 255.0;
			float alpha2 = alpha * mMirror.y();

			alpha *= mMirror.x();

			const unsigned int colorT = Renderer::convertColor((mColorShift & 0xffffff00) + (unsigned char)(255.0*alpha));
			const unsigned int colorB = Renderer::convertColor((mColorShift & 0xffffff00) + (unsigned char)(255.0*alpha2));

			int h = mVertices[1].pos.y() - mVertices[0].pos.y();
			if (mReflectOnBorders)
				h = mTargetSize.y();

			Renderer::Vertex mirrorVertices[4];

			mirrorVertices[0] = {
				{ mVertices[0].pos.x(), mVertices[0].pos.y() + h },
				{ mVertices[0].tex.x(), mVertices[1].tex.y() },
				colorT };

			mirrorVertices[1] = {
				{ mVertices[1].pos.x(), mVertices[1].pos.y() + h },
				{ mVertices[1].tex.x(), mVertices[0].tex.y() },
				colorB };

			mirrorVertices[2] = {
				{ mVertices[2].pos.x(), mVertices[2].pos.y() + h },
				{ mVertices[2].tex.x(), mVertices[3].tex.y() },
				colorT };

			mirrorVertices[3] = {
				{ mVertices[3].pos.x(), mVertices[3].pos.y() + h },
				{ mVertices[3].tex.x(), mVertices[2].tex.y() },
				colorB };

			Renderer::drawTriangleStrips(&mirrorVertices[0], 4);
		}

		Renderer::bindTexture(0);
	}

	GuiComponent::renderChildren(trans);
}

void ImageComponent::fadeIn(bool textureLoaded)
{
	if (!mAllowFading)
		return;

	if (!mForceLoad)
	{
		if (!textureLoaded)
		{
			// Start the fade if this is the first time we've encountered the unloaded texture
			if (!mFading)
			{
				// Start with a zero opacity and flag it as fading
				mFadeOpacity = 0;
				mFading = true;
				// Set the colours to be translucent
			//	mColorShift = (mColorShift >> 8 << 8) | 0;			
			//	mColorShiftEnd = (mColorShiftEnd >> 8 << 8) | 0;
			}
		}
		else if (mFading && textureLoaded)
		{
			// The texture is loaded and we need to fade it in. The fade is based on the frame rate
			// and is 1/4 second if running at 60 frames per second although the actual value is not
			// that important
			int opacity = mFadeOpacity + 255 / 15;
			// See if we've finished fading
			if (opacity >= 255)
			{
				mFadeOpacity = 255;
				mFading = false;
			}
			else
			{
				mFadeOpacity = (unsigned char)opacity;
			}
			// Apply the combination of the target opacity and current fade
		//	float newOpacity = (float)mOpacity * ((float)mFadeOpacity / 255.0f);
	//		mColorShift = (mColorShift >> 8 << 8) | (unsigned char)newOpacity;		
	//		mColorShiftEnd = (mColorShiftEnd >> 8 << 8) | (unsigned char)newOpacity;
		}
	}
}

bool ImageComponent::hasImage()
{
	return (bool)mTexture;
}

void ImageComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	applyThemeWithType(theme, view, element, properties, "image");
}

void ImageComponent::applyThemeWithType(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties, const std::string& expectedType)
{
	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, expectedType);
	if(!elem)
	{
		return;
	}

	setStoryboards(elem->storyboards);
	loadBindingExpressions(elem);

	if (elem->has("linearSmooth"))
		mLinear = elem->get<bool>("linearSmooth");

	setScaleOrigin(elem->has("scaleOrigin") ? elem->get<Vector2f>("scaleOrigin") : Vector2f(0.0f, 0.0f));

	// before <path> so the image is loaded with it
	setSaturation(elem->has("saturation") ? elem->get<float>("saturation") : 1.0f);

	if (elem->has("autoFade"))
		setAllowFading(elem->get<bool>("autoFade"));

	// normalized coordinates are relative to the parent's client area (AmberELEC)
	Vector4f clientRect = getParentClientRect();
	Vector2f scale = Vector2f(clientRect.z(), clientRect.w());
	Vector2f offset = Vector2f(clientRect.x(), clientRect.y());
	
	if(properties & POSITION && elem->has("pos"))
	{
		Vector2f pos = elem->get<Vector2f>("pos");
		mSourceBounds.x() = pos.x(); mSourceBounds.y() = pos.y();
		Vector2f denormalized = pos * scale + offset;
		setPosition(Vector3f(denormalized.x(), denormalized.y(), 0));
	}

	// ImageComponent reimplements applyTheme() from scratch instead of delegating to
	// GuiComponent::applyTheme(), so it needs its own handling of the standalone <x>/<y>
	// tags (as opposed to the combined <pos> tag above) - previously missing here, which
	// silently dropped image positioning for any theme using <x>/<y> instead of <pos>
	if (properties & POSITION && elem->has("x"))
	{
		mSourceBounds.x() = elem->get<float>("x");
		float x = mSourceBounds.x() * scale.x() + offset.x();
		setPosition(Vector3f(x, mPosition.y(), mPosition.z()));
	}

	if (properties & POSITION && elem->has("y"))
	{
		mSourceBounds.y() = elem->get<float>("y");
		float y = mSourceBounds.y() * scale.y() + offset.y();
		setPosition(Vector3f(mPosition.x(), y, mPosition.z()));
	}

	// padding before sizing, the image is fitted inside it
	applyThemePadding(elem);

	if(properties & ThemeFlags::SIZE)
	{
		if(elem->has("size"))
		{
			auto sz = elem->get<Vector2f>("size");
			mSourceBounds.z() = sz.x(); mSourceBounds.w() = sz.y();
			if (sz.x() == 0 && sz.y() != 0 && Settings::getInstance()->getInt("ScreenRotate") != 0)
			{
				sz.x() = sz.y();
				setMinSize(sz * scale);
			}
			else if (sz.y() == 0 && sz.x() != 0 && Settings::getInstance()->getInt("ScreenRotate") != 0)
			{
				sz.y() = sz.x();
				setMinSize(sz * scale);
			}
			else
				setResize(sz * scale);
		}
		else if(elem->has("maxSize"))
		{
			Vector2f sz = elem->get<Vector2f>("maxSize");
			mSourceBounds.z() = sz.x(); mSourceBounds.w() = sz.y();
			setMaxSize(sz * scale);
		}
		else if(elem->has("minSize"))
		{
			Vector2f sz = elem->get<Vector2f>("minSize");
			mSourceBounds.z() = sz.x(); mSourceBounds.w() = sz.y();
			setMinSize(sz * scale);
		}
	}
	
	// position + size also implies origin
	if((properties & ORIGIN || (properties & POSITION && properties & ThemeFlags::SIZE)) && elem->has("origin"))
		setOrigin(elem->get<Vector2f>("origin"));

	if(elem->has("default")) {
		setDefaultImage(elem->get<std::string>("default"));
	}



	if(properties & PATH && elem->has("path"))
	{
		auto path = elem->get<std::string>("path");
		if (ResourceManager::getInstance()->fileExists(path))
		{
			bool tile = (elem->has("tile") && elem->get<bool>("tile"));
			setImage(path, tile/*, Vector2f(mTargetSize.x(), mTargetSize.y())*/);
		}
	}

	if (properties & COLOR)
	{
		if (elem->has("color"))
			setColorShift(elem->get<unsigned int>("color"));

		if (elem->has("colorEnd"))
			setColorShiftEnd(elem->get<unsigned int>("colorEnd"));

		if (elem->has("gradientType"))
			setColorGradientHorizontal(elem->get<std::string>("gradientType").compare("horizontal"));

		if (elem->has("reflexion"))
			mMirror = elem->get<Vector2f>("reflexion");
		else
			mMirror = Vector2f::Zero();

		if (elem->has("reflexionOnFrame"))
			mReflectOnBorders = elem->get<bool>("reflexionOnFrame");
		else
			mReflectOnBorders = false;
	}

	if(properties & ThemeFlags::ROTATION) 
	{
		if(elem->has("rotation"))
			setRotationDegrees(elem->get<float>("rotation"));

		if(elem->has("rotationOrigin"))
			setRotationOrigin(elem->get<Vector2f>("rotationOrigin"));

		if (elem->has("flipX"))
			setFlipX(elem->get<bool>("flipX"));

		if (elem->has("flipY"))
			setFlipY(elem->get<bool>("flipY"));
	}

	if (properties & ALIGNMENT && elem->has("horizontalAlignment"))
	{
		std::string str = elem->get<std::string>("horizontalAlignment");
		if (str == "left")
			setHorizontalAlignment(ALIGN_LEFT);
		else if (str == "right")
			setHorizontalAlignment(ALIGN_RIGHT);
		else
			setHorizontalAlignment(ALIGN_CENTER);
	}

	if (properties & ALIGNMENT && elem->has("verticalAlignment"))
	{
		std::string str = elem->get<std::string>("verticalAlignment");
		if (str == "top")
			setVerticalAlignment(ALIGN_TOP);
		else if (str == "bottom")
			setVerticalAlignment(ALIGN_BOTTOM);
		else
			setVerticalAlignment(ALIGN_CENTER);
	}

	if(properties & ThemeFlags::Z_INDEX && elem->has("zIndex"))
		setZIndex(elem->get<float>("zIndex"));
	else
		setZIndex(getDefaultZIndex());

	if (properties & ALIGNMENT && elem->has("roundCorners"))
		mRoundCorners = elem->get<float>("roundCorners");

	if(properties & ThemeFlags::VISIBLE && elem->has("visible"))
		setVisible(elem->get<bool>("visible"));
	else
		setVisible(true);

	applyThemeOpacityAndScale(elem, properties);
	applyThemeLayout(elem, properties);
}

// nested theme element: follow the parent's client area (AmberELEC)
void ImageComponent::recalcLayout()
{
	if (!mThemedChild || getParent() == nullptr)
		return;

	Vector4f clientRect = getParent()->getClientRect();
	Vector2f newPos = Vector2f(mSourceBounds.x() * clientRect.z() + clientRect.x(), mSourceBounds.y() * clientRect.w() + clientRect.y());
	Vector2f newSize = Vector2f(mSourceBounds.z() * clientRect.z(), mSourceBounds.w() * clientRect.w());

	if (Vector2f(mPosition.x(), mPosition.y()) != newPos)
		setPosition(newPos.x(), newPos.y(), mPosition.z());

	if (mTargetSize != newSize)
	{
		if (mTargetIsMax)
			setMaxSize(newSize);
		else if (mTargetIsMin)
			setMinSize(newSize);
		else
			setResize(newSize);
	}
}

std::vector<HelpPrompt> ImageComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> ret;
	ret.push_back(HelpPrompt(BUTTON_OK, _("SELECT")));
	return ret;
}



void ImageComponent::setPlaylist(std::shared_ptr<IPlaylist> playList)
{
	mPlaylist = playList;
	if (mPlaylist == nullptr)
		return;

	auto image = mPlaylist->getNextItem();
	if (!image.empty())
		setImage(image);
}

void ImageComponent::onShow()
{
	GuiComponent::onShow();

	if (!mShowing && mPlaylist != nullptr && !mPath.empty())
	{
		auto item = mPlaylist->getNextItem();
		if (!item.empty())
			setImage(item, false, getMaxSizeInfo());
	}

	mShowing = true;	
}

void ImageComponent::onHide()
{
	GuiComponent::onHide();
	mShowing = false;	
}


void ImageComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mPlaylist != nullptr && mShowing)
	{
		mPlaylistTimer += deltaTime;

		if (mPlaylistTimer >= 10000)
		{
			auto item = mPlaylist->getNextItem();
			if (!item.empty())
				setImage(item, false, getMaxSizeInfo());

			mPlaylistTimer = 0.0;
		}
	}
}

bool ImageComponent::isTiled()
{ 
	return mTexture != nullptr && mTexture->isTiled(); 
}

// theme bindings (AmberELEC ImageComponent::getProperty / setProperty)
ThemeData::ThemeElement::Property ImageComponent::getProperty(const std::string name)
{
	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if (name == "maxSize" || name == "minSize")
		return Vector2f(mSize.x() / scale.x(), mSize.y() / scale.y());
	else if (name == "color")
		return mColorShift;
	else if (name == "colorEnd")
		return mColorShiftEnd;
	else if (name == "roundCorners")
		return mRoundCorners;
	else if (name == "path")
		return mPath;
	else if (name == "default")
		return mDefaultPath;
	else if (name == "autoFade")
		return mAllowFading;
	else if (name == "saturation")
		return mSaturation;

	return GuiComponent::getProperty(name);
}

void ImageComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	typedef ThemeData::ThemeElement::Property::PropertyType PropType;

	Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if (value.type == PropType::Float && name == "w")
	{
		mTargetSize = Vector2f(value.f * scale.x(), mTargetSize.y());
		resize();
	}
	else if (value.type == PropType::Float && name == "h")
	{
		mTargetSize = Vector2f(mTargetSize.x(), value.f * scale.y());
		resize();
	}
	else if (value.type == PropType::Pair && (name == "maxSize" || name == "minSize"))
	{
		mTargetSize = Vector2f(value.v.x() * scale.x(), value.v.y() * scale.y());
		resize();
	}
	else if (value.type == PropType::Int && name == "color")
		setColorShift(value.i);
	else if (value.type == PropType::Int && name == "colorEnd")
		setColorShiftEnd(value.i);
	else if (value.type == PropType::Float && name == "saturation")
		setSaturation(value.f);
	else if (value.type == PropType::Bool && name == "autoFade")
		setAllowFading(value.b);
	else if (value.type == PropType::Float && name == "roundCorners")
		setRoundCorners(value.f);
	else if (value.type == PropType::String && name == "default")
		setDefaultImage(value.s);
	else if (value.type == PropType::String && name == "path")
		setImage(value.s);
	else
		GuiComponent::setProperty(name, value);
}
