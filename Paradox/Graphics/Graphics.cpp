#include "Graphics.h"
#include "../Helpers.h"
#include <dxgi1_6.h>
#include <d3d12.h>
#include "dxc/dxcapi.h"
#include "dxc/dxcapi.use.h"
#include <wrl.h>

using namespace Microsoft::WRL;

Graphics::Graphics(Config config)
	:
	m_D3DParams(config.width, config.height, true)
{}

Graphics::~Graphics()
{
	Shutdown();
}

void Graphics::Init(HWND hwnd)
{
	InitializeShaderCompiler();

	CreateDevice();
	CreateCommandQueue();
	CreateCommandAllocator();
	CreateFence();
	CreateSwapChain(hwnd);
	CreateCommandList();
	ResetCommandList();

	CreateDescriptorHeaps();

	Model tempModel;
	Vertex vert;
	vert.position = XMFLOAT3{ 0, 0, 0 };
	vert.uv = XMFLOAT2{ 0, 0 };
	tempModel.vertices = { vert };
	tempModel.indices = { 0 };
	CreateVertexBuffer(tempModel);
	CreateIndexBuffer(tempModel);
}

void Graphics::Shutdown()
{

}

void Graphics::InitializeShaderCompiler()
{
	HRESULT hr = m_ShaderCompilerInfo.DxcDllHelper.Initialize();
	Helpers::Validate(hr, L"Failed to initialize DxCDllSupport!");

	hr = m_ShaderCompilerInfo.DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &m_ShaderCompilerInfo.compiler);
	Helpers::Validate(hr, L"Failed to create DxcCompiler!");

	hr = m_ShaderCompilerInfo.DxcDllHelper.CreateInstance(CLSID_DxcLibrary, &m_ShaderCompilerInfo.library);
	Helpers::Validate(hr, L"Failed to create DxcLibrary!");
}

void Graphics::CreateDevice()
{
#if defined (_DEBUG)
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
#endif

	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_D3DObjects.factory));
	Helpers::Validate(hr, L"Failed to create factory!");

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != m_D3DObjects.factory->EnumAdapters1(adapterIndex, &m_D3DObjects.adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 adapterDesc = {};
		m_D3DObjects.adapter->GetDesc1(&adapterDesc);

		if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue; 
		}

		if (SUCCEEDED(D3D12CreateDevice(m_D3DObjects.adapter, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device5), (void**)&m_D3DObjects.device)))
		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS5 features = {};
			HRESULT hr = m_D3DObjects.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features, sizeof(features));

			if (FAILED(hr) || features.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
			{
				m_D3DObjects.device->Release();
				m_D3DObjects.device = nullptr;
				continue;
			}

#if NAME_D3D_RESOURCES
			m_D3DObjects.devices->SetName(L"DXR Enabled Device");
			printf("Running on DXGI Adapter %S\n", adapterDesc.Description);
#endif
			break;
		}

		if (m_D3DObjects.device == nullptr)
		{
			Helpers::Validate(E_FAIL, L"Failed to create ray tracing device!");
		}
	}
}

void Graphics::CreateCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT hr = m_D3DObjects.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_D3DObjects.commandQueue));
	Helpers::Validate(hr, L"Failed to create command queue!");

#if NAME_D3D_RESOURCES
	m_D3DObjects.commandQueue->SetName(L"D3D12 Command Queue");
#endif
}

void Graphics::CreateCommandAllocator()
{
	for (int n = 0; n < 2; ++n)
	{
		HRESULT hr = m_D3DObjects.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_D3DObjects.commandAllocators[n]));
		Helpers::Validate(hr, L"Failed to create command allocator!");
	}

#if NAME_D3D_RESOURCES
	if (n = 0) m_D3DObjects.commandAllocators[n]->SetName(L"D3D12 Command Allocator 0");
	else m_D3DObjects.commandAllocators[n]->SetName(L"D3D12 Command Allocator 1");
#endif
}

void Graphics::CreateFence()
{
	HRESULT hr = m_D3DObjects.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_D3DObjects.fence));
	Helpers::Validate(hr, L"Failed to create fence!");

#if NAME_D3D_RESOURCES
	m_D3DObjects.fence->SetName(L"D3D12 Fence");
#endif

	m_D3DValues.fenceValues[m_D3DValues.frameIndex]++;

	m_D3DObjects.fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	
	if (m_D3DObjects.fenceEvent == nullptr)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		Helpers::Validate(hr, L"Failed to create fence event!");
	}
}

void Graphics::CreateSwapChain(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.BufferCount = m_D3DValues.swapChainBufferCount;
	desc.Width = m_D3DParams.width;
	desc.Height = m_D3DParams.height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.SampleDesc.Count = 1;

	IDXGISwapChain1* swapChain;
	HRESULT hr = m_D3DObjects.factory->CreateSwapChainForHwnd(m_D3DObjects.commandQueue, hwnd, &desc, nullptr, nullptr, &swapChain);
	Helpers::Validate(hr, L"Failed to create swap chain!");

	hr = m_D3DObjects.factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	Helpers::Validate(hr, L"Failed to make window association");

	hr = swapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&m_D3DObjects.swapChain));
	Helpers::Validate(hr, L"Failed to cast swap chain!");

	swapChain->Release();
	m_D3DValues.frameIndex = m_D3DObjects.swapChain->GetCurrentBackBufferIndex();
}

void Graphics::CreateCommandList()
{
	HRESULT hr = m_D3DObjects.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_D3DObjects.commandAllocators[m_D3DValues.frameIndex], nullptr, IID_PPV_ARGS(&m_D3DObjects.commandList));
	hr = m_D3DObjects.commandList->Close();
	Helpers::Validate(hr, L"Failed to create command list!");

#if NAME_D3D_RESOURCES
	m_D3DObjects.commandList->SetName(L"D3D12 Command List");
#endif
}

void Graphics::ResetCommandList()
{
	HRESULT hr = m_D3DObjects.commandAllocators[m_D3DValues.frameIndex]->Reset();
	Helpers::Validate(hr, L"Failed to reset command allocator!");

	hr = m_D3DObjects.commandList->Reset(m_D3DObjects.commandAllocators[m_D3DValues.frameIndex], nullptr);
	Helpers::Validate(hr, L"Failed to reset command list!");
}

void Graphics::CreateDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = m_D3DValues.swapChainBufferCount;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = m_D3DObjects.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_D3DResources.rtvHeap));
	Helpers::Validate(hr, L"Failed to create RTV descriptor heap!");

#if NAME_D3D_RESOURCES
	m_D3DResources.rtvHeap->SetName(L"Render Target View Descriptor Heap");
#endif

	m_D3DValues.rtvDescSize = m_D3DObjects.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void Graphics::CreateBackBufferRtv()
{
	HRESULT hr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_D3DResources.rtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (UINT n = 0; n < m_D3DValues.swapChainBufferCount; ++n)
	{
		hr = m_D3DObjects.swapChain->GetBuffer(n, IID_PPV_ARGS(&m_D3DObjects.backBuffer[n]));
		Helpers::Validate(hr, L"Failed to create back buffers!");

		m_D3DObjects.device->CreateRenderTargetView(m_D3DObjects.backBuffer[n], nullptr, rtvHandle);

#if NAME_D3D_RESOURCES
		if (n == 0) m_D3DObjects.backBuffer[0]->SetName(L"Back Buffer 0");
		m_D3DObjects.backBuffer[1]->SetName(L"Back Buffer 1");
#endif

		rtvHandle.ptr += m_D3DValues.rtvDescSize;

	}
}

void Graphics::CreateBuffer(D3D12BufferCreateInfo& info, ID3D12Resource** ppResource)
{
	D3D12_HEAP_PROPERTIES heapDesc = {};
	heapDesc.Type = info.heapType;
	heapDesc.CreationNodeMask = 1;
	heapDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = info.alignment;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Width = info.size;
	resourceDesc.Flags = info.flags;

	HRESULT hr = m_D3DObjects.device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &resourceDesc, info.state, nullptr, IID_PPV_ARGS(ppResource));
	Helpers::Validate(hr, L"Failed to create buffer resource!");
}

void Graphics::CreateVertexBuffer(Model& model)
{
	D3D12BufferCreateInfo info((UINT)model.vertices.size() * sizeof(Vertex), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	CreateBuffer(info, &m_D3DResources.vertexBuffer);

#if NAME_D3D_RESOURCES
	m_D3DResources.vertexBuffer->SetName(L"Vertex Buffer");
#endif

	UINT8* pVertexDataBegin;
	D3D12_RANGE readRange = {};
	HRESULT hr = m_D3DResources.vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
	Helpers::Validate(hr, L"Failed to map vertex buffer");

	memcpy(pVertexDataBegin, model.vertices.data(), info.size);
	m_D3DResources.vertexBuffer->Unmap(0, nullptr);

	m_D3DResources.vertexBufferView.BufferLocation = m_D3DResources.vertexBuffer->GetGPUVirtualAddress();
	m_D3DResources.vertexBufferView.SizeInBytes = static_cast<UINT>(info.size);
	m_D3DResources.vertexBufferView.StrideInBytes = sizeof(Vertex);
}

void Graphics::CreateIndexBuffer(Model& model)
{
	D3D12BufferCreateInfo info((UINT)model.indices.size() * sizeof(UINT), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	CreateBuffer(info, &m_D3DResources.indexBuffer);

#if NAME_D3D_RESOURCES
	m_D3DResources.indexBuffer->SetName(L"Index Buffer");
#endif

	UINT8* pIndexDataBegin;
	D3D12_RANGE readRange = {};
	HRESULT hr = m_D3DResources.indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
	Helpers::Validate(hr, L"Failed to map index buffer");

	memcpy(pIndexDataBegin, model.indices.data(), info.size);
	m_D3DResources.indexBuffer->Unmap(0, nullptr);

	m_D3DResources.indexBufferView.BufferLocation = m_D3DResources.indexBuffer->GetGPUVirtualAddress();
	m_D3DResources.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_D3DResources.indexBufferView.SizeInBytes = static_cast<UINT>(info.size);
}