#include "FFR.h"

#include "Settings.h"
#include "resource.h"
#include "Utils.h"

using Microsoft::WRL::ComPtr;
using namespace d3d_render_utils;

namespace {

	struct FoveationVars {
		uint32_t targetEyeWidth;
		uint32_t targetEyeHeight;
		uint32_t optimizedEyeWidth;
		uint32_t optimizedEyeHeight;
		float focusPositionX;
		float focusPositionY;
		float foveationScaleX;
		float foveationScaleY;

		float boundStartX;
		float boundStartY;
		float distortedWidth;
		float distortedHeight;
	};

	const float DEG_TO_RAD = (float)M_PI / 180;

#define INVERSE_DISTORTION_FN(a) atan(a);
	const float INVERSE_DISTORTION_DERIVATIVE_IN_0 = 1; // d(atan(0))/dx = 1

	float CalcBoundStart(float focusPos, float fovScale) {
		return INVERSE_DISTORTION_FN(-focusPos * fovScale);
	}

	float CalcBoundEnd(float focusPos, float fovScale) {
		return INVERSE_DISTORTION_FN((1.f - focusPos) * fovScale);
	}

	float CalcDistortedDimension(float focusPos, float fovScale) {
		float boundEnd = CalcBoundEnd(focusPos, fovScale);
		float boundStart = CalcBoundStart(focusPos, fovScale);
		return boundEnd - boundStart;
	}

	float CalcOptimalDimensionForWarp(float scale, float distortedDim, float originalDim) {
		float inverseDistortionDerivative = INVERSE_DISTORTION_DERIVATIVE_IN_0 * scale;
		float gradientOnFocus = inverseDistortionDerivative / distortedDim;
		return originalDim / gradientOnFocus;
	}

	float Align4Normalized(float scale, float originalDim) {
		return float(int(scale * originalDim / 4.f) * 4) / originalDim;
	}

	float CalcOptimalDimensionForSlicing(float scale, float originalDim) {
		return (1. + 3. * scale) / 4. * originalDim + 6;
	}

	FoveationVars CalculateFoveationVars() {
		auto mode = Settings::Instance().m_foveationMode;

		float targetEyeWidth = (float)Settings::Instance().m_renderWidth / 2;
		float targetEyeHeight = (float)Settings::Instance().m_renderHeight;

		auto leftEye = Settings::Instance().m_eyeFov[0];

		// left and right side screen plane width with unit focal
		float leftHalfWidth = tan(leftEye.left * DEG_TO_RAD);
		float rightHalfWidth = tan(leftEye.right * DEG_TO_RAD);
		// foveated center X assuming screen plane with unit width
		float focusPositionX = leftHalfWidth / (leftHalfWidth + rightHalfWidth);
		// align focus position to a number of pixel multiple of 4 to avoid blur and artifacts
		if (mode == FFR_MODE_SLICES) {
			focusPositionX = Align4Normalized(focusPositionX, targetEyeWidth);
		}


		// NB: swapping top/bottom fov
		float topHalfHeight = tan(leftEye.bottom * DEG_TO_RAD);
		float bottomHalfHeight = tan(leftEye.top * DEG_TO_RAD);
		float focusPositionY = topHalfHeight / (topHalfHeight + bottomHalfHeight);
		//focusPositionY = Align4Normalized(focusPositionY, targetEyeHeight);
		if (mode == FFR_MODE_SLICES) {
			focusPositionY = Align4Normalized(focusPositionY, targetEyeHeight);
		}

		//calculate foveation scale such as the "area" of the foveation region remains equal to (mFoveationStrengthMean)^2
		// solve for {foveationScaleX, foveationScaleY}:
		// /{ foveationScaleX * foveationScaleY = (mFoveationStrengthMean)^2
		// \{ foveationScaleX / foveationScaleY = 1 / mFoveationShapeRatio
		// then foveationScaleX := foveationScaleX / (targetEyeWidth / targetEyeHeight) to compensate for non square frame.
		float strengthMean = Settings::Instance().m_foveationStrengthMean;
		float strengthRatio = Settings::Instance().m_foveationShapeRatio;
		if (mode == FFR_MODE_SLICES) {
			strengthMean = 1.f / (strengthMean + 1.f);
			strengthRatio = 1.f / strengthRatio;
		}
		float scaleCoeff = strengthMean * sqrt(strengthRatio);
		float foveationScaleX = scaleCoeff / strengthRatio / (targetEyeWidth / targetEyeHeight);
		float foveationScaleY = scaleCoeff;
		if (mode == FFR_MODE_SLICES) {
			foveationScaleX = Align4Normalized(foveationScaleX, targetEyeWidth);
			foveationScaleY = Align4Normalized(foveationScaleY, targetEyeHeight);
		}

		float optimizedEyeWidth = 0;
		float optimizedEyeHeight = 0;
		float boundStartX = 0;
		float boundStartY = 0;
		float distortedWidth = 0;
		float distortedHeight = 0;

		if (mode == FFR_MODE_SLICES) {
			optimizedEyeWidth = CalcOptimalDimensionForSlicing(foveationScaleX, targetEyeWidth);
			optimizedEyeHeight = CalcOptimalDimensionForSlicing(foveationScaleY, targetEyeHeight);
		}
		else if (mode == FFR_MODE_WARP) {
			boundStartX = CalcBoundStart(focusPositionX, foveationScaleX);
			boundStartY = CalcBoundStart(focusPositionY, foveationScaleY);

			distortedWidth = CalcDistortedDimension(focusPositionX, foveationScaleX);
			distortedHeight = CalcDistortedDimension(focusPositionY, foveationScaleY);

			optimizedEyeWidth = CalcOptimalDimensionForWarp(foveationScaleX, distortedWidth, targetEyeWidth);
			optimizedEyeHeight = CalcOptimalDimensionForWarp(foveationScaleY, distortedHeight, targetEyeHeight);
		}

		// round the frame dimensions to a number of pixel multiple of 32 for the encoder
		auto optimizedEyeWidthAligned = (uint32_t)ceil(optimizedEyeWidth / 32.f) * 32;
		auto optimizedEyeHeightAligned = (uint32_t)ceil(optimizedEyeHeight / 32.f) * 32;

		//throw MakeException("%f %f %f %f %d", targetEyeHeight, focusPositionY, foveationScaleY, optimizedEyeHeight, optimizedEyeHeightAligned);

		return { (uint32_t)targetEyeWidth, (uint32_t)targetEyeHeight, optimizedEyeWidthAligned, optimizedEyeHeightAligned,
			focusPositionX, focusPositionY, foveationScaleX, foveationScaleY,
			boundStartX, boundStartY, distortedWidth, distortedHeight };
	}
}


void FFR::GetOptimizedResolution(uint32_t* width, uint32_t* height) {
	auto fovVars = CalculateFoveationVars();
	*width = fovVars.optimizedEyeWidth * 2;
	*height = fovVars.optimizedEyeHeight;
}

FFR::FFR(ID3D11Device* device) : mDevice(device) {}

void FFR::Initialize(ID3D11Texture2D* compositionTexture) {
	auto fovVars = CalculateFoveationVars();
	ComPtr<ID3D11Buffer> foveatedRenderingBuffer = CreateBuffer(mDevice.Get(), fovVars);

	std::vector<uint8_t> quadShaderCSO;
	if (!ReadBinaryResource(quadShaderCSO, IDR_QUAD_SHADER)) {
		throw MakeException(L"Failed to load resource for IDR_QUAD_SHADER.");
	}
	mQuadVertexShader = CreateVertexShader(mDevice.Get(), quadShaderCSO);

	mOptimizedTexture = CreateTexture(mDevice.Get(), fovVars.optimizedEyeWidth * 2,
		fovVars.optimizedEyeHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

	switch (Settings::Instance().m_foveationMode) {
	case FFR_MODE_DISABLED:
		mOptimizedTexture = compositionTexture;
		break;
	case FFR_MODE_SLICES:
	{
		std::vector<uint8_t> compressSlicesShaderCSO;
		if (!ReadBinaryResource(compressSlicesShaderCSO, IDR_COMPRESS_SLICES_SHADER)) {
			throw MakeException(L"Failed to load resource for IDR_COMPRESS_SLICES_SHADER.");
		}

		auto compressSlicesPipeline = RenderPipeline(mDevice.Get());
		compressSlicesPipeline.Initialize({ compositionTexture }, mQuadVertexShader.Get(),
			compressSlicesShaderCSO, mOptimizedTexture.Get(), foveatedRenderingBuffer.Get());

		mPipelines.push_back(compressSlicesPipeline);
		break;
	}
	case FFR_MODE_WARP:

		/*ComPtr<ID3D11Texture2D> horizontalBlurredTexture = CreateTexture(mDevice.Get(),
			fovVars.targetEyeWidth * 2, fovVars.targetEyeHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);*/

		//std::vector<uint8_t> horizontalBlurShaderCSO;
		//if (!ReadBinaryResource(horizontalBlurShaderCSO, IDR_HORIZ_BLUR_SHADER)) {
		//	throw MakeException(L"Failed to load resource for IDR_HORIZ_BLUR_SHADER.");
		//}

		std::vector<uint8_t> distortionShaderCSO;
		if (!ReadBinaryResource(distortionShaderCSO, IDR_DISTORTION_SHADER)) {
			throw MakeException(L"Failed to load resource for IDR_DISTORTION_SHADER.");
		}

		/*mHorizontalBlurPipeline.Initialize({ compositionTexture }, mQuadVertexShader.Get(),
			horizontalBlurShaderCSO, horizontalBlurredTexture.Get(), foveatedRenderingBuffer.Get());*/

		auto distortPipeline = RenderPipeline(mDevice.Get());
		distortPipeline.Initialize({ compositionTexture }, mQuadVertexShader.Get(),
			distortionShaderCSO, mOptimizedTexture.Get(), foveatedRenderingBuffer.Get());

		//mPipelines.push_back(horizontalBlurPipeline);
		mPipelines.push_back(distortPipeline);
		break;
	}
}

void FFR::Render() {
	for (auto &p : mPipelines) {
		p.Render();
	}
}

ID3D11Texture2D* FFR::GetOutputTexture() {
	return mOptimizedTexture.Get();
}