#include "Graphics.h"
#include "../Helpers.h"
#include <dxgi1_6.h>
#include <d3d12.h>
#include "dxc/dxcapi.h"
#include "dxc/dxcapi.use.h"

#define ALIGN(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)
#define SAFE_RELEASE(x) { if (x) { x->Release(); x = NULL; } }

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
	LoadModel("models/cinema.obj", model, material);
	InitializeShaderCompiler();

	CreateDevice();
	CreateCommandQueue();
	CreateCommandAllocator();
	CreateFence();
	CreateSwapChain(hwnd);
	CreateCommandList();
	ResetCommandList();

	CreateDescriptorHeaps();
	CreateBackBufferRtv();
	CreateVertexBuffer(model);
	CreateIndexBuffer(model);

	CreateTexture(material);

	CreateSceneCB();
	CreateMaterialConstantBuffer(material);

	CreateBottomLevelAS();
	CreateTopLevelAS();
	CreateDXROutput();

	CreateDescriptorHeaps(model);
	CreateRayGenProgram();
	CreateMissProgram();
	CreateClosestHitProgram();
	CreatePipelineStateObject();
	CreateShaderTable();

	m_D3DObjects.commandList->Close();
	ID3D12CommandList* pGraphicsList = { m_D3DObjects.commandList };
	m_D3DObjects.commandQueue->ExecuteCommandLists(1, &pGraphicsList);

	WaitForGPU();
	ResetCommandList();
}

void Graphics::Update()
{
	UpdateSceneCB();
}

void Graphics::Render()
{
	BuildCommandList();
	Present();
	MoveToNextFrame();
	ResetCommandList();
}

void Graphics::Shutdown()
{
	WaitForGPU();
	CloseHandle(m_D3DObjects.fenceEvent);

	DestroyDXRObjects();
	DestroyD3D12Resources();
	DestroyShaders();
	DestroyD3D12Objects();
}

namespace std
{
	void hash_combine(size_t& seed, size_t hash)
	{
		hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= hash;
	}

	template<> struct hash<Vertex>
	{
		size_t operator()(Vertex const& vertex) const
		{
			size_t seed = 0;
			hash<float> hasher;
			hash_combine(seed, hasher(vertex.position.x));
			hash_combine(seed, hasher(vertex.position.y));
			hash_combine(seed, hasher(vertex.position.z));

			hash_combine(seed, hasher(vertex.uv.x));
			hash_combine(seed, hasher(vertex.uv.y));

			return seed;
		}
	};
}

using namespace std;

void Graphics::LoadModel(std::string filepath, Model& model, Material& material)
{
	using namespace tinyobj;
	attrib_t attrib;
	std::vector<shape_t> shapes;
	std::vector<material_t> materials;
	std::string err;

	if (!LoadObj(&attrib, &shapes, &materials, &err, filepath.c_str(), "materials\\"))
	{
		throw std::runtime_error(err);
	}

	material.name = materials[0].name;
	material.texturePath = materials[0].diffuse_texname;


	std::unordered_map<Vertex, uint32_t> uniqueVertices = {};
	for (const auto &shape : shapes)
	{
		for (const auto &index : shape.mesh.indices)
		{
			Vertex vertex = {};
			vertex.position =
			{
				attrib.vertices[3 * index.vertex_index + 2],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 0]
			};

			vertex.uv =
			{
				attrib.texcoords[2 * index.texcoord_index + 0],
				1 - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(model.vertices.size());
				model.vertices.push_back(vertex);
			}

			model.indices.push_back(uniqueVertices[vertex]);
		};
	}
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
	ID3D12Debug* debugController;
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

void Graphics::CreateTexture(Material& material)
{
	TextureInfo texture = Helpers::LoadTexture(material.texturePath);
	material.textureResolution = static_cast<float>(texture.width);

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Width = texture.width;
	textureDesc.Height = texture.height;
	textureDesc.MipLevels = 1;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	HRESULT hr = m_D3DObjects.device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_D3DResources.texture));
	Helpers::Validate(hr, L"Failed to create texture!");

#if NAME_D3D_RESOURCES
	m_D3DResources.texture->SetName(L"Texture");
#endif

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = (texture.width * texture.height * texture.stride);
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;

	hr = m_D3DObjects.device->CreateCommittedResource(&UploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_D3DResources.textureUploadResource));
	Helpers::Validate(hr, L"Failed to create texture upload heap!");

#if NAME_D3D_RESOURCES
	m_D3DResources.textureUploadResource->SetName(L"Texture Upload Buffer");
#endif

	UploadTexture(m_D3DResources.texture, m_D3DResources.textureUploadResource, texture);
}

void Graphics::UploadTexture(ID3D12Resource* destResource, ID3D12Resource* srcResource, const TextureInfo& texture)
{
	UINT8* pData;
	HRESULT hr = srcResource->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	memcpy(pData, texture.pixels.data(), texture.width * texture.height * texture.stride);
	srcResource->Unmap(0, nullptr);

	D3D12_SUBRESOURCE_FOOTPRINT subresource = {};
	subresource.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	subresource.Width = texture.width;
	subresource.Height = texture.height;
	subresource.RowPitch = texture.width * texture.stride;
	subresource.Depth = 1;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	footprint.Offset = texture.offset;
	footprint.Footprint = subresource;

	D3D12_TEXTURE_COPY_LOCATION source = {};
	source.pResource = srcResource;
	source.PlacedFootprint = footprint;
	source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

	D3D12_TEXTURE_COPY_LOCATION destination = {}; 
	destination.pResource = destResource;
	destination.SubresourceIndex = 0;
	destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	m_D3DObjects.commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = destResource;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	m_D3DObjects.commandList->ResourceBarrier(1, &barrier);
}

void Graphics::CreateConstantBuffer(ID3D12Resource** buffer, UINT64 size)
{
	D3D12BufferCreateInfo bufferInfo((size + 255) & ~255, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	CreateBuffer(bufferInfo, buffer);
}

void Graphics::CreateSceneCB()
{
	CreateConstantBuffer(&m_D3DResources.sceneCB, sizeof(SceneCB));

#if NAME_D3D_RESOURCES
	m_D3DResources.sceneCB->SetName(L"View Constant Buffer");
#endif

	HRESULT hr = m_D3DResources.sceneCB->Map(0, nullptr, reinterpret_cast<void**>(&m_D3DResources.sceneCBStart));
	Helpers::Validate(hr, L"Failed to map view constant buffer");

	memcpy(m_D3DResources.sceneCBStart, &m_D3DResources.sceneCBData, sizeof(m_D3DResources.sceneCBData));
}

void Graphics::CreateMaterialConstantBuffer(const Material& material)
{
	CreateConstantBuffer(&m_D3DResources.materialCB, sizeof(MaterialCB));

#if NAME_D3D_RESOURCES
	m_D3DResources.materialCB->SetName(L"Material Constant Buffer");
#endif

	m_D3DResources.materialCBData.resolution = XMFLOAT4(material.textureResolution, 0.0f, 0.0f, 0.0f);

	HRESULT hr = m_D3DResources.materialCB->Map(0, nullptr, reinterpret_cast<void**>(&m_D3DResources.materialCBStart));
	Helpers::Validate(hr, L"Failed to map material constant buffer");

	memcpy(m_D3DResources.materialCBStart, &m_D3DResources.materialCBData, sizeof(m_D3DResources.materialCB));
};

void Graphics::CreateBottomLevelAS()
{
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.VertexBuffer.StartAddress = m_D3DResources.vertexBuffer->GetGPUVirtualAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = m_D3DResources.vertexBufferView.StrideInBytes;
	geometryDesc.Triangles.VertexCount = static_cast<uint32_t>(model.vertices.size());
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometryDesc.Triangles.IndexBuffer = m_D3DResources.indexBuffer->GetGPUVirtualAddress();
	geometryDesc.Triangles.IndexCount = static_cast<uint32_t>(model.indices.size());
	geometryDesc.Triangles.IndexFormat = m_D3DResources.indexBufferView.Format;
	geometryDesc.Triangles.Transform3x4 = 0;
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
	ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	ASInputs.pGeometryDescs = &geometryDesc;
	ASInputs.NumDescs = 1;
	ASInputs.Flags = buildFlags;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
	m_D3DObjects.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

	ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);
	ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);

	D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CreateBuffer(bufferInfo, &m_DXRObjects.BLAS.pSratch);
#if NAME_D3D_RESOURCES
	m_DXRObjects.BLAS.pScratch->SetName(L"DXR BLAS Scratch");
#endif

	bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CreateBuffer(bufferInfo, &m_DXRObjects.BLAS.pResult);
#if NAME_D3D_RESOURCES
	m_DXRObjects.BLAS.pResult->SetName(L"DXR BLAS");
#endif

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = ASInputs;
	buildDesc.ScratchAccelerationStructureData = m_DXRObjects.BLAS.pSratch->GetGPUVirtualAddress();
	buildDesc.DestAccelerationStructureData = m_DXRObjects.BLAS.pResult->GetGPUVirtualAddress();

	m_D3DObjects.commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

	D3D12_RESOURCE_BARRIER uavBarrier;
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = m_DXRObjects.BLAS.pResult;
	uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	
	m_D3DObjects.commandList->ResourceBarrier(1, &uavBarrier);
};

void Graphics::CreateTopLevelAS()
{
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
	instanceDesc.InstanceID = 0;
	instanceDesc.InstanceContributionToHitGroupIndex = 0;
	instanceDesc.InstanceMask = 0xFF;
	instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
	instanceDesc.AccelerationStructure = m_DXRObjects.BLAS.pResult->GetGPUVirtualAddress();
	instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;

	D3D12BufferCreateInfo instanceBufferInfo = {};
	instanceBufferInfo.size = sizeof(instanceDesc);
	instanceBufferInfo.heapType = D3D12_HEAP_TYPE_UPLOAD;
	instanceBufferInfo.flags = D3D12_RESOURCE_FLAG_NONE;
	instanceBufferInfo.state = D3D12_RESOURCE_STATE_GENERIC_READ;
	CreateBuffer(instanceBufferInfo, &m_DXRObjects.TLAS.pInstanceDesc);
#if NAME_D3D_RESOURCES
	m_DXRObjects.TLAS.pInstanceDesc->SetName(L"DXR TLAS Instance Descriptors");
#endif

	UINT8* pData;
	m_DXRObjects.TLAS.pInstanceDesc->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	memcpy(pData, &instanceDesc, sizeof(instanceDesc));
	m_DXRObjects.TLAS.pInstanceDesc->Unmap(0, nullptr);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
	ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	ASInputs.InstanceDescs = m_DXRObjects.TLAS.pInstanceDesc->GetGPUVirtualAddress();
	ASInputs.NumDescs = 1;
	ASInputs.Flags = buildFlags;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
	m_D3DObjects.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

	ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
	ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

	m_DXRObjects.tlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

	D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CreateBuffer(bufferInfo, &m_DXRObjects.TLAS.pSratch);
#if NAME_D3D_RESOURCES
	m_DXRObjects.TLAS.pScratch->SetName(L"DXR TLAS Scratch");
#endif

	bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CreateBuffer(bufferInfo, &m_DXRObjects.TLAS.pResult);
#if NAME_D3D_RESOURCES
	m_DXRObjects.TLAS.pScratch->SetName(L"DXR TLAS");
#endif

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = ASInputs;
	buildDesc.ScratchAccelerationStructureData = m_DXRObjects.TLAS.pSratch->GetGPUVirtualAddress();
	buildDesc.DestAccelerationStructureData = m_DXRObjects.TLAS.pResult->GetGPUVirtualAddress();

	m_D3DObjects.commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

	D3D12_RESOURCE_BARRIER uavBarrier;
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = m_DXRObjects.TLAS.pResult;
	uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	m_D3DObjects.commandList->ResourceBarrier(1, &uavBarrier);
}

void Graphics::CreateDXROutput()
{
	D3D12_RESOURCE_DESC desc = {};
	desc.DepthOrArraySize = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	desc.Width = m_D3DParams.width;
	desc.Height = m_D3DParams.height;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	HRESULT hr = m_D3DObjects.device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_D3DResources.DXROutputBuffer));
	Helpers::Validate(hr, L"Failed to create DXR Output Buffer");

#if NAME_D3D_RESOURCES
	m_D3DResources.DXROutputBuffer->SetName(L"DXR Output Buffer");
#endif
}

void Graphics::CreateDescriptorHeaps(const Model& model)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 7;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	HRESULT hr = m_D3DObjects.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_D3DResources.descriptorHeap));
	Helpers::Validate(hr, L"Failed to create descriptor heap!");

	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_D3DResources.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	UINT handleIncrement = m_D3DObjects.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

#if NAME_D3D_RESOURCES
	m_D3DResources.descriptorHeap->SetName(L"DXR Descriptor Heap");
#endif

	// Create View Constant Buffer CBV
	D3D12_CONSTANT_BUFFER_VIEW_DESC sceneCBDesc = {};
	sceneCBDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(m_D3DResources.sceneCBData));
	sceneCBDesc.BufferLocation = m_D3DResources.sceneCB->GetGPUVirtualAddress();

	m_D3DObjects.device->CreateConstantBufferView(&sceneCBDesc, handle);

	// Create MaterialCB CBV
	sceneCBDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(m_D3DResources.materialCBData));
	sceneCBDesc.BufferLocation = m_D3DResources.materialCB->GetGPUVirtualAddress();

	handle.ptr += handleIncrement;
	m_D3DObjects.device->CreateConstantBufferView(&sceneCBDesc, handle);

	// Create DXR Output Buffer UAV 
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	handle.ptr += handleIncrement;
	m_D3DObjects.device->CreateUnorderedAccessView(m_D3DResources.DXROutputBuffer, nullptr, &uavDesc, handle);

	// Create the DXR Top Level Acceleration Structure SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = m_DXRObjects.TLAS.pResult->GetGPUVirtualAddress();

	handle.ptr += handleIncrement;
	m_D3DObjects.device->CreateShaderResourceView(nullptr, &srvDesc, handle);

	// Create the Index Buffer SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC indexSrvDesc = {};
	indexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	indexSrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	indexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	indexSrvDesc.Buffer.StructureByteStride = 0;
	indexSrvDesc.Buffer.FirstElement = 0;
	indexSrvDesc.Buffer.NumElements = (static_cast<UINT>(model.indices.size()) * sizeof(UINT)) / sizeof(float);
	indexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	m_D3DObjects.device->CreateShaderResourceView(m_D3DResources.indexBuffer, &indexSrvDesc, handle);

	// Create the Vertex Buffer SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrvDesc = {};
	vertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	vertexSrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	vertexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	vertexSrvDesc.Buffer.StructureByteStride = 0;
	vertexSrvDesc.Buffer.FirstElement = 0;
	vertexSrvDesc.Buffer.NumElements = (static_cast<UINT>(model.vertices.size()) * sizeof(Vertex)) / sizeof(float);
	vertexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	m_D3DObjects.device->CreateShaderResourceView(m_D3DResources.vertexBuffer, &vertexSrvDesc, handle);

	// Create the material texture SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSrvDesc = {};
	textureSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	textureSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureSrvDesc.Texture2D.MipLevels = 1;
	textureSrvDesc.Texture2D.MostDetailedMip = 0;
	textureSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	m_D3DObjects.device->CreateShaderResourceView(m_D3DResources.texture, &textureSrvDesc, handle);
}

ID3D12RootSignature* Graphics::CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
{
	ID3DBlob* sig;
	ID3DBlob* error;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &error);
	Helpers::Validate(hr, L"Failed to serialize root signature!");

	ID3D12RootSignature* pRootSig;
	hr = m_D3DObjects.device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
	Helpers::Validate(hr, L"Failed to create root signature!");

	SAFE_RELEASE(sig);
	SAFE_RELEASE(error);

	return pRootSig;
}

void Graphics::CompileShader(D3D12ShaderInfo& info, IDxcBlob** blob)
{
	HRESULT hr;
	UINT code(0);
	IDxcBlobEncoding* pShaderText(nullptr);

	//Load and encode the shader file
	hr = m_ShaderCompilerInfo.library->CreateBlobFromFile(info.filename, &code, &pShaderText);
	Helpers::Validate(hr, L"Failed to create blob from shader file!");

	//Create the compiler include handler
	IDxcIncludeHandler* dxcIncludeHandler;
	hr = m_ShaderCompilerInfo.library->CreateIncludeHandler(&dxcIncludeHandler);
	Helpers::Validate(hr, L"Failed to create include handler!");

	//Compile the shader
	IDxcOperationResult* result;
	hr = m_ShaderCompilerInfo.compiler->Compile(
		pShaderText,
		info.filename,
		info.entryPoint,
		info.targetProfile,
		info.arguments,
		info.argCount,
		info.defines,
		info.defineCount,
		dxcIncludeHandler,
		&result);

	Helpers::Validate(hr, L"Failed to compile shader!");

	//Verify the result
	result->GetStatus(&hr);
	if (FAILED(hr))
	{
		IDxcBlobEncoding* error;
		hr = result->GetErrorBuffer(&error);
		Helpers::Validate(hr, L"Failed to get shader compiler error buffer!");

		//Convert error blob to a string
		vector<char> infoLog(error->GetBufferSize() + 1);
		memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
		infoLog[error->GetBufferSize()] = 0;

		string errorMsg = "Shader Compiler Error: \n";
		errorMsg.append(infoLog.data());

		MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
		return;
	}

	hr = result->GetResult(blob);
	Helpers::Validate(hr, L"Failed to get shader blob result!");
}

void Graphics::CompileShader(RtProgram &program)
{
	CompileShader(program.info, &program.blob);
	program.SetBytecode();
}


void Graphics::CreateRayGenProgram()
{
	m_DXRObjects.rgs = RtProgram(D3D12ShaderInfo(L"shaders\\RayGen.hlsl", L"", L"lib_6_3"));
	CompileShader(m_DXRObjects.rgs);

	//Describe the ray generation root signature
	D3D12_DESCRIPTOR_RANGE ranges[3];

	ranges[0].BaseShaderRegister = 0;
	ranges[0].NumDescriptors = 2;
	ranges[0].RegisterSpace = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;

	ranges[1].BaseShaderRegister = 0;
	ranges[1].NumDescriptors = 1;
	ranges[1].RegisterSpace = 0;
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].OffsetInDescriptorsFromTableStart = 2;

	ranges[2].BaseShaderRegister = 0;
	ranges[2].NumDescriptors = 4;
	ranges[2].RegisterSpace = 0;
	ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[2].OffsetInDescriptorsFromTableStart = 3;

	D3D12_ROOT_PARAMETER param0 = {};
	param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param0.DescriptorTable.NumDescriptorRanges = _countof(ranges);
	param0.DescriptorTable.pDescriptorRanges = ranges;

	D3D12_ROOT_PARAMETER rootParams[1] = { param0 };

	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = _countof(rootParams);
	rootDesc.pParameters = rootParams;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	//Create the root signature
	m_DXRObjects.rgs.pRootSignature = CreateRootSignature(rootDesc);
#if NAME_D3D_RESOURCES
	m_DXRObjects.rgs.pRootSignature->SetName(L"DXR RGS Root Signature");
#endif
}

void Graphics::CreateMissProgram()
{
	m_DXRObjects.miss = RtProgram(D3D12ShaderInfo(L"shaders\\Miss.hlsl", L"", L"lib_6_3"));
	CompileShader(m_DXRObjects.miss);
}

void Graphics::CreateClosestHitProgram()
{
	m_DXRObjects.hit = HitProgram(L"Hit");
	m_DXRObjects.hit.chs = RtProgram(D3D12ShaderInfo(L"shaders\\ClosestHit.hlsl", L"", L"lib_6_3"));
	CompileShader(m_DXRObjects.hit.chs);
}

void Graphics::CreatePipelineStateObject()
{
	// Need 10 subobjects:
	// 1 for RGS program
	// 1 for Miss program
	// 1 for CHS program
	// 1 for Hit Group
	// 2 for RayGen Root Signature (root-signature and association)
	// 2 for Shader Config (config and association)
	// 1 for Global Root Signature
	// 1 for Pipeline Config

	UINT index = 0;
	vector<D3D12_STATE_SUBOBJECT> subobjects;
	subobjects.resize(10);

	// Add state subobject for the RGS
	D3D12_EXPORT_DESC rgsExportDesc = {};
	rgsExportDesc.Name = L"RayGen_12";
	rgsExportDesc.ExportToRename = L"RayGen";
	rgsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

	D3D12_DXIL_LIBRARY_DESC rgsLibDesc = {};
	rgsLibDesc.DXILLibrary.BytecodeLength = m_DXRObjects.rgs.blob->GetBufferSize();
	rgsLibDesc.DXILLibrary.pShaderBytecode = m_DXRObjects.rgs.blob->GetBufferPointer();
	rgsLibDesc.NumExports = 1;
	rgsLibDesc.pExports = &rgsExportDesc;

	D3D12_STATE_SUBOBJECT rgs = {};
	rgs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	rgs.pDesc = &rgsLibDesc;

	subobjects[index++] = rgs;

	// Add state subobject for the Miss shader
	D3D12_EXPORT_DESC missExportDesc = {};
	missExportDesc.Name = L"Miss_5";
	missExportDesc.ExportToRename = L"Miss";
	missExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

	D3D12_DXIL_LIBRARY_DESC msLibDesc = {};
	msLibDesc.DXILLibrary.BytecodeLength = m_DXRObjects.miss.blob->GetBufferSize();
	msLibDesc.DXILLibrary.pShaderBytecode = m_DXRObjects.miss.blob->GetBufferPointer();
	msLibDesc.NumExports = 1;
	msLibDesc.pExports = &missExportDesc;

	D3D12_STATE_SUBOBJECT ms = {};
	ms.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	ms.pDesc = &msLibDesc;

	subobjects[index++] = ms;

	// Add state subobject for the Closest Hit shader
	D3D12_EXPORT_DESC chsExportDesc = {};
	chsExportDesc.Name = L"ClosestHit_76";
	chsExportDesc.ExportToRename = L"ClosestHit";
	chsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

	D3D12_DXIL_LIBRARY_DESC chsLibDesc = {};
	chsLibDesc.DXILLibrary.BytecodeLength = m_DXRObjects.hit.chs.blob->GetBufferSize();
	chsLibDesc.DXILLibrary.pShaderBytecode = m_DXRObjects.hit.chs.blob->GetBufferPointer();
	chsLibDesc.NumExports = 1;
	chsLibDesc.pExports = &chsExportDesc;

	D3D12_STATE_SUBOBJECT chs = {};
	chs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	chs.pDesc = &chsLibDesc;

	subobjects[index++] = chs;

	// Add state subobject for the hit group
	D3D12_HIT_GROUP_DESC hitGroupDesc = {};
	hitGroupDesc.ClosestHitShaderImport = L"ClosestHit_76";
	hitGroupDesc.HitGroupExport = L"HitGroup";

	D3D12_STATE_SUBOBJECT hitGroup = {};
	hitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	hitGroup.pDesc = &hitGroupDesc;

	subobjects[index++] = hitGroup;

	// Add a state subobject for the shader payload configuration
	D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
	shaderDesc.MaxPayloadSizeInBytes = sizeof(XMFLOAT4); // RGB and hitT
	shaderDesc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;

	D3D12_STATE_SUBOBJECT shaderConfigObject = {};
	shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
	shaderConfigObject.pDesc = &shaderDesc;

	subobjects[index++] = shaderConfigObject;

	// Create a list of the shader export names that use the payload
	const WCHAR* shaderExports[] = { L"RayGen_12", L"Miss_5", L"HitGroup" };

	// Add a state subobject for the association between shaders and the payload
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
	shaderPayloadAssociation.NumExports = _countof(shaderExports);
	shaderPayloadAssociation.pExports = shaderExports;
	shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

	D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
	shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;

	subobjects[index++] = shaderPayloadAssociationObject;

	// Add a state subobject for the shared root signature
	D3D12_STATE_SUBOBJECT rayGenRootSigObject = {};
	rayGenRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	rayGenRootSigObject.pDesc = &m_DXRObjects.rgs.pRootSignature;

	subobjects[index++] = rayGenRootSigObject;

	// Create a list of the shader export names that use the root signature
	const WCHAR* rootSigExports[] = { L"RayGen_12", L"HitGroup", L"Miss_5" };

	// Add a state subobject for the association between the RayGen shader and the local root signature
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenShaderRootSigAssociation = {};
	rayGenShaderRootSigAssociation.NumExports = _countof(rootSigExports);
	rayGenShaderRootSigAssociation.pExports = rootSigExports;
	rayGenShaderRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

	D3D12_STATE_SUBOBJECT rayGenShaderRootSigAssociationObject = {};
	rayGenShaderRootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	rayGenShaderRootSigAssociationObject.pDesc = &rayGenShaderRootSigAssociation;

	subobjects[index++] = rayGenShaderRootSigAssociationObject;

	D3D12_STATE_SUBOBJECT globalRootSig = {};
	globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	globalRootSig.pDesc = &m_DXRObjects.miss.pRootSignature;

	subobjects[index++] = globalRootSig;

	// Add a state subobject for the ray tracing pipeline config
	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
	pipelineConfig.MaxTraceRecursionDepth = 1;

	D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
	pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	pipelineConfigObject.pDesc = &pipelineConfig;

	subobjects[index++] = pipelineConfigObject;

	// Describe the Ray Tracing Pipeline State Object
	D3D12_STATE_OBJECT_DESC pipelineDesc = {};
	pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	pipelineDesc.NumSubobjects = static_cast<UINT>(subobjects.size());
	pipelineDesc.pSubobjects = subobjects.data();

	// Create the RT Pipeline State Object (RTPSO)
	HRESULT hr = m_D3DObjects.device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&m_DXRObjects.rtpso));
	Helpers::Validate(hr, L"Failed to create Raytracing Pipeline State Object!");
#if NAME_D3D_RESOURCES
	m_DXRObjects.rtpso->SetName(L"DXR Pipeline State Object");
#endif

	// Get the RTPSO properties
	hr = m_DXRObjects.rtpso->QueryInterface(IID_PPV_ARGS(&m_DXRObjects.rtpsoInfo));
	Helpers::Validate(hr, L"Failed to get RTPSO info object");
}

void Graphics::CreateShaderTable()
{
	/*
	The Shader Table layout is:
		Entry 0 - Ray Generation shader
		Entry 1 - Miss shader
		Entry 2 - Closest Hit shader
	All shader records in the Shader Table must have the same size, so we set shader record size based on the largest required entry.
	The ray generation program requires the largest entry:
		32 bytes - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
	+	8 bytes  - a CBV/SRV/UAV descriptor table point (64-bits)
	=	40 bytes ->> aligns to 64 bytes
	The entry size must be aligned up to D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT
	*/
	
	uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	uint32_t shaderTableSize = 0;

	m_DXRObjects.shaderTableRecordSize = shaderIdSize;
	m_DXRObjects.shaderTableRecordSize += 8;
	m_DXRObjects.shaderTableRecordSize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, m_DXRObjects.shaderTableRecordSize);

	shaderTableSize = (m_DXRObjects.shaderTableRecordSize * 3);
	shaderTableSize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, shaderTableSize);

	// Create the shader table buffer
	D3D12BufferCreateInfo bufferInfo(shaderTableSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	CreateBuffer(bufferInfo, &m_DXRObjects.shaderTable);
#if NAME_D3D_RESOURCES
	m_DXRObjects.shaderTable->SetName(L"DXR Shader Table");
#endif

	// Map the buffer
	uint8_t* pData = 0;
	HRESULT hr = m_DXRObjects.shaderTable->Map(0, nullptr, (void**)&pData);
	Helpers::Validate(hr, L"Failed to map shader table!");

	// Shader Record 0 - Ray Generation program and local root parameter data (descriptor table with constant buffer and index buffer / vertex buffer pointers)
	memcpy(pData, m_DXRObjects.rtpsoInfo->GetShaderIdentifier(L"RayGen_12"), shaderIdSize);

	// Set the root parameter data. Point to the start of the descriptor heap.
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = m_D3DResources.descriptorHeap->GetGPUDescriptorHandleForHeapStart();

	// Shader Record 1 - Miss program (no local root arguments to set)
	pData += m_DXRObjects.shaderTableRecordSize;
	memcpy(pData, m_DXRObjects.rtpsoInfo->GetShaderIdentifier(L"Miss_5"), shaderIdSize);

	// Shader Record 2 - Closest Hit program and local root parameter data (descriptor table with constant buffer and index buffer / vertex buffer pointers)
	pData += m_DXRObjects.shaderTableRecordSize;
	memcpy(pData, m_DXRObjects.rtpsoInfo->GetShaderIdentifier(L"HitGroup"), shaderIdSize);

	// Set the root parameter data. Point to start to descriptor heap.
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = m_D3DResources.descriptorHeap->GetGPUDescriptorHandleForHeapStart();

	// Unmap
	m_DXRObjects.shaderTable->Unmap(0, nullptr);
}

void Graphics::WaitForGPU()
{
	// Schedule a signal command in the queue
	HRESULT hr = m_D3DObjects.commandQueue->Signal(m_D3DObjects.fence, m_D3DValues.fenceValues[m_D3DValues.frameIndex]);
	Helpers::Validate(hr, L"Failed to signal fence!");

	// Wait until the fence has been processed
	hr = m_D3DObjects.fence->SetEventOnCompletion(m_D3DValues.fenceValues[m_D3DValues.frameIndex], m_D3DObjects.fenceEvent);
	Helpers::Validate(hr, L"Failed to set fence event!");

	WaitForSingleObjectEx(m_D3DObjects.fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame
	m_D3DValues.fenceValues[m_D3DValues.frameIndex]++;
}

void Graphics::UpdateSceneCB()
{
	const float rotationSpeed = 0.005f;
	XMMATRIX view, invView;
	XMFLOAT3 eye, focus, up;
	float aspect, fov;

	m_D3DResources.eyeAngle.x += rotationSpeed;

/*
	float x = 2.f * cosf(m_D3DResources.eyeAngle.x);
	float y = 0.f;
	float z = 2.25f + 2.f * sinf(m_D3DResources.eyeAngle.x);

	focus = XMFLOAT3(0.f, 0.f, 0.f);
*/

	float x = 8.f * cosf(m_D3DResources.eyeAngle.x);
	float y = 1.5f + 1.5f * cosf(m_D3DResources.eyeAngle.x);
	float z = 8.f + 2.25f * sinf(m_D3DResources.eyeAngle.x);

	focus = XMFLOAT3(0.f, 1.75f, 0.f);

	eye = XMFLOAT3(x, y, z);
	up = XMFLOAT3(0.f, 1.f, 0.f);

	aspect = (float)m_D3DParams.width / (float)m_D3DParams.height;
	fov = 65.f * (XM_PI / 180.f);

	m_D3DResources.rotationOffset += rotationSpeed;

	view = DirectX::XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&focus), XMLoadFloat3(&up));
	invView = DirectX::XMMatrixInverse(NULL, view);

	m_D3DResources.sceneCBData.view = DirectX::XMMatrixTranspose(invView);
	m_D3DResources.sceneCBData.viewOriginAndTanHalfFovY = XMFLOAT4(eye.x, eye.y, eye.z, tanf(fov * 0.5f));
	m_D3DResources.sceneCBData.resolution = XMFLOAT2((float)m_D3DParams.width, (float)m_D3DParams.height);
	memcpy(m_D3DResources.sceneCBStart, &m_D3DResources.sceneCBData, sizeof(m_D3DResources.sceneCBData));
}

void Graphics::BuildCommandList()
{
	D3D12_RESOURCE_BARRIER OutputBarriers[2] = {};
	D3D12_RESOURCE_BARRIER CounterBarriers[2] = {};
	D3D12_RESOURCE_BARRIER UAVBarriers[3] = {};

	// Transition the back buffer to a copy destination
	OutputBarriers[0].Transition.pResource = m_D3DObjects.backBuffer[m_D3DValues.frameIndex];
	OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	OutputBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// Transition the DXR Output Buffer to a copy source
	OutputBarriers[1].Transition.pResource = m_D3DResources.DXROutputBuffer;
	OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	OutputBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// Wait for the transitions to complete
	m_D3DObjects.commandList->ResourceBarrier(2, OutputBarriers);

	// Set the UAV/SRV/CBV and sampler heaps
	ID3D12DescriptorHeap* ppHeaps[] = { m_D3DResources.descriptorHeap };
	m_D3DObjects.commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// Dispatch Rays
	D3D12_DISPATCH_RAYS_DESC desc = {};
	desc.RayGenerationShaderRecord.StartAddress = m_DXRObjects.shaderTable->GetGPUVirtualAddress();
	desc.RayGenerationShaderRecord.SizeInBytes = m_DXRObjects.shaderTableRecordSize;

	desc.MissShaderTable.StartAddress = m_DXRObjects.shaderTable->GetGPUVirtualAddress() + m_DXRObjects.shaderTableRecordSize;
	desc.MissShaderTable.SizeInBytes = m_DXRObjects.shaderTableRecordSize;
	desc.MissShaderTable.StrideInBytes = m_DXRObjects.shaderTableRecordSize;

	desc.HitGroupTable.StartAddress = m_DXRObjects.shaderTable->GetGPUVirtualAddress() + (2 * m_DXRObjects.shaderTableRecordSize);
	desc.HitGroupTable.SizeInBytes = m_DXRObjects.shaderTableRecordSize;
	desc.HitGroupTable.StrideInBytes = m_DXRObjects.shaderTableRecordSize;

	desc.Width = m_D3DParams.width;
	desc.Height = m_D3DParams.height;
	desc.Depth = 1;

	m_D3DObjects.commandList->SetPipelineState1(m_DXRObjects.rtpso);
	m_D3DObjects.commandList->DispatchRays(&desc);

	// Transition DXR output to a copy source
	OutputBarriers[1].Transition.pResource = m_D3DResources.DXROutputBuffer;
	OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

	// Wait for the transitions to complete
	m_D3DObjects.commandList->ResourceBarrier(1, &OutputBarriers[1]);

	// Copy the DXR Output Buffer to the back buffer
	m_D3DObjects.commandList->CopyResource(m_D3DObjects.backBuffer[m_D3DValues.frameIndex], m_D3DResources.DXROutputBuffer);

	// Transition back buffer to present
	OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	// Wait for transitions to complete
	m_D3DObjects.commandList->ResourceBarrier(1, &OutputBarriers[0]);

	// Submit the command list and wait for the GPU to idle
	SubmitCommandList();
	WaitForGPU();
}

void Graphics::SubmitCommandList()
{
	m_D3DObjects.commandList->Close();

	ID3D12CommandList* pGraphicsList = { m_D3DObjects.commandList };
	m_D3DObjects.commandQueue->ExecuteCommandLists(1, &pGraphicsList);
	m_D3DValues.fenceValues[m_D3DValues.frameIndex]++;
	m_D3DObjects.commandQueue->Signal(m_D3DObjects.fence, m_D3DValues.fenceValues[m_D3DValues.frameIndex]);
}

void Graphics::Present()
{
	HRESULT hr = m_D3DObjects.swapChain->Present(m_D3DParams.vsync, 0);
	if (FAILED(hr))
	{
		hr = m_D3DObjects.device->GetDeviceRemovedReason();
		Helpers::Validate(hr, L"Failed to present!");
	}
}


// Prepare to render the next frame
void Graphics::MoveToNextFrame()
{
	// Schedule a Signal command in the queue
	const UINT64 currentFenceValue = m_D3DValues.fenceValues[m_D3DValues.frameIndex];
	HRESULT hr = m_D3DObjects.commandQueue->Signal(m_D3DObjects.fence, currentFenceValue);
	Helpers::Validate(hr, L"Failed to signal the command queue!");

	// Update the frame index
	m_D3DValues.frameIndex = m_D3DObjects.swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is
	if (m_D3DObjects.fence->GetCompletedValue() < m_D3DValues.fenceValues[m_D3DValues.frameIndex])
	{
		hr = m_D3DObjects.fence->SetEventOnCompletion(m_D3DValues.fenceValues[m_D3DValues.frameIndex], m_D3DObjects.fenceEvent);
		Helpers::Validate(hr, L"Failed to set fence value!");

		WaitForSingleObjectEx(m_D3DObjects.fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame
	m_D3DValues.fenceValues[m_D3DValues.frameIndex] = currentFenceValue + 1;
}

void Graphics::DestroyDXRObjects()
{
	SAFE_RELEASE(m_DXRObjects.TLAS.pSratch);
	SAFE_RELEASE(m_DXRObjects.TLAS.pResult);
	SAFE_RELEASE(m_DXRObjects.TLAS.pInstanceDesc);
	SAFE_RELEASE(m_DXRObjects.BLAS.pSratch);
	SAFE_RELEASE(m_DXRObjects.BLAS.pResult);
	SAFE_RELEASE(m_DXRObjects.BLAS.pInstanceDesc);
	SAFE_RELEASE(m_DXRObjects.shaderTable);
	SAFE_RELEASE(m_DXRObjects.rgs.blob);
	SAFE_RELEASE(m_DXRObjects.rgs.pRootSignature);
	SAFE_RELEASE(m_DXRObjects.miss.blob);
	SAFE_RELEASE(m_DXRObjects.hit.chs.blob);
	SAFE_RELEASE(m_DXRObjects.rtpso);
	SAFE_RELEASE(m_DXRObjects.rtpsoInfo);
}

void Graphics::DestroyD3D12Resources()
{
	if (m_D3DResources.sceneCB) m_D3DResources.sceneCB->Unmap(0, nullptr);
	if (m_D3DResources.sceneCBStart) m_D3DResources.sceneCBStart = nullptr;
	if (m_D3DResources.materialCB) m_D3DResources.materialCB->Unmap(0, nullptr);
	if (m_D3DResources.materialCBStart) m_D3DResources.materialCBStart = nullptr;

	SAFE_RELEASE(m_D3DResources.DXROutputBuffer);
	SAFE_RELEASE(m_D3DResources.vertexBuffer);
	SAFE_RELEASE(m_D3DResources.indexBuffer);
	SAFE_RELEASE(m_D3DResources.sceneCB);
	SAFE_RELEASE(m_D3DResources.materialCB);
	SAFE_RELEASE(m_D3DResources.rtvHeap);
	SAFE_RELEASE(m_D3DResources.descriptorHeap);
	SAFE_RELEASE(m_D3DResources.texture);
	SAFE_RELEASE(m_D3DResources.textureUploadResource);
};

void Graphics::DestroyShaders()
{
	SAFE_RELEASE(m_ShaderCompilerInfo.compiler);
	SAFE_RELEASE(m_ShaderCompilerInfo.library);
	m_ShaderCompilerInfo.DxcDllHelper.Cleanup();
}

void Graphics::DestroyD3D12Objects()
{
	SAFE_RELEASE(m_D3DObjects.fence);
	SAFE_RELEASE(m_D3DObjects.backBuffer[1]);
	SAFE_RELEASE(m_D3DObjects.backBuffer[0]);
	SAFE_RELEASE(m_D3DObjects.swapChain);
	SAFE_RELEASE(m_D3DObjects.commandAllocators[0]);
	SAFE_RELEASE(m_D3DObjects.commandAllocators[1]);
	SAFE_RELEASE(m_D3DObjects.commandQueue);
	SAFE_RELEASE(m_D3DObjects.commandList);
	SAFE_RELEASE(m_D3DObjects.device);
	SAFE_RELEASE(m_D3DObjects.adapter);
	SAFE_RELEASE(m_D3DObjects.factory);
}