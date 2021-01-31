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

struct D3D12Values
{
	UINT swapChainBufferCount = 2;
	UINT64 fenceValues[2] = { 0, 0 };
	UINT frameIndex = 0;
	UINT rtvDescSize = 0;
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
	ID3D12Resource* backBuffer[2] = { nullptr, nullptr };

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

	ID3D12Resource* vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	ID3D12Resource* indexBuffer;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
};

struct D3D12BufferCreateInfo
{
	UINT64 size = 0;
	UINT64 alignment = 0;
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

	D3D12BufferCreateInfo() {};

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags) 
		:
		size(InSize),
		flags(InFlags)
	{};

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState)
		:
		size(InSize),
		flags(InFlags),
		state(InState)
	{};

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_STATES InState)
		:
		size(InSize),
		heapType(InHeapType),
		state(InState)
	{};

	D3D12BufferCreateInfo(UINT64 InSize, UINT64 InAlignment, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState)
		:
		size(InSize),
		alignment(InAlignment),
		heapType(InHeapType),
		flags(InFlags),
		state(InState)
	{};
};