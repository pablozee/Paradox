#pragma once
#include <Windows.h>
#include "dxc/dxcapi.h"
#include "dxc/dxcapi.use.h"
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>

struct D3D12Params
{
	D3D12Params(unsigned int width,
				unsigned int height,
				bool vsync)
		:
		width(width),
		height(height),
		vsync(vsync)
	{}

	unsigned int width;
	unsigned int height;
	bool vsync = true;
};

struct D3D12Objects
{
	IDXGIFactory4* factory = nullptr;
	IDXGIAdapter1* adapter = nullptr;
	ID3D12Device5* device = nullptr;
	ID3D12CommandQueue* commandQueue = nullptr;
	
};

struct D3D12ShaderCompilerInfo
{
	dxc::DxcDllSupport DxcDllHelper;
	IDxcCompiler* compiler = nullptr;
	IDxcLibrary* library = nullptr;
};