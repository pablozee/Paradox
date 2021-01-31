#pragma once
#include <Windows.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <string>
#include <stdexcept>
#include <unordered_map>

#include "Structures.h"

namespace Helpers
{
	void Validate(HRESULT hr, LPWSTR message)
	{
		if (FAILED(hr))
		{
			MessageBox(NULL, message, L"Error", MB_OK);
			PostQuitMessage(EXIT_FAILURE);
		}
	}

	void FormatTexture(TextureInfo& info, UINT8* pixels)
	{
		const UINT numPixels = (info.width * info.height);
		const UINT oldStride = info.stride;
		const UINT oldSize = (numPixels * info.stride);

		const UINT newStride = 4;
		const UINT newSize = (numPixels * newStride);
		info.pixels.resize(newSize);

		for (UINT i = 0; i < numPixels; ++i)
		{
			info.pixels[i * newStride] = pixels[i * oldStride];
			info.pixels[i * newStride + 1] = pixels[i * oldStride + 1];
			info.pixels[i * newStride + 2] = pixels[i * oldStride + 2];
			info.pixels[i * newStride + 3] = 0xFF;
		}

		info.stride = newStride;
	}

	TextureInfo LoadTexture(std::string filepath)
	{
		TextureInfo result = {};

		UINT8* pixels = stbi_load(filepath.c_str(), &result.width, &result.height, &result.stride, STBI_default);
		if (!pixels)
		{
			throw std::runtime_error("Error: failed to load image!");
		}

		FormatTexture(result, pixels);
		stbi_image_free(pixels);
		return result;
	}
};