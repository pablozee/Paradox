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
	ID3D12CommandAllocator* commandAllocators[2] = { nullptr, nullptr };
	ID3D12GraphicsCommandList* commandList = nullptr;

	ID3D12Fence* fence = nullptr;
	HANDLE fenceEvent;

	IDXGISwapChain3* swapChain = nullptr;

};

struct D3D12Values
{
	UINT swapChainBufferCount = 2;
	UINT64 fenceValues[2] = { 0, 0 };
	UINT frameIndex = 0;
	UINT rtvDescSize = 0;
};

struct D3D12ShaderCompilerInfo
{
	dxc::DxcDllSupport DxcDllHelper;
	IDxcCompiler* compiler = nullptr;
	IDxcLibrary* library = nullptr;
};

struct D3D12Resources
{
	ID3D12DescriptorHeap* rtvHeap;
};