#include "Graphics.h"
#include "../Helpers.h"
#include <dxgi1_6.h>
#include <d3d12.h>
#include "dxc/dxcapi.h"
#include "dxc/dxcapi.use.h"

#define ALIGN(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)

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

	CreateVertexBuffer(model);
	CreateIndexBuffer(model);

	CreateTexture(material);

	CreateViewCB();
	CreateMaterialConstantBuffer(material);

	CreateBottomLevelAS();
	CreateTopLevelAS();
	CreateDXROutput();

	CreateDescriptorHeaps(model);

}

void Graphics::Shutdown()
{

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
	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
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
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	m_D3DObjects.commandList->ResourceBarrier(1, &barrier);
}

void Graphics::CreateConstantBuffer(ID3D12Resource** buffer, UINT64 size)
{
	D3D12BufferCreateInfo bufferInfo((size + 255) & ~255, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	CreateBuffer(bufferInfo, buffer);
}

void Graphics::CreateViewCB()
{
	CreateConstantBuffer(&m_D3DResources.viewCB, sizeof(ViewCB));

#if NAME_D3D_RESOURCES
	m_D3DResources.viewCB->SetName(L"View Constant Buffer");
#endif

	HRESULT hr = m_D3DResources.viewCB->Map(0, nullptr, reinterpret_cast<void**>(&m_D3DResources.viewCBStart));
	Helpers::Validate(hr, L"Failed to map view constant buffer");

	memcpy(m_D3DResources.viewCBStart, &m_D3DResources.viewCBData, sizeof(m_D3DResources.viewCBData));
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

	m_D3DObjects.commandList->BuildRaytracingAccelerationStructure(&buildDesc, 1u, nullptr);

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
	D3D12_CONSTANT_BUFFER_VIEW_DESC viewCBDesc = {};
	viewCBDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(m_D3DResources.viewCBData));
	viewCBDesc.BufferLocation = m_D3DResources.viewCB->GetGPUVirtualAddress();

	m_D3DObjects.device->CreateConstantBufferView(&viewCBDesc, handle);

	// Create MaterialCB CBV
	viewCBDesc.SizeInBytes = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(m_D3DResources.materialCBData));
	viewCBDesc.BufferLocation = m_D3DResources.materialCB->GetGPUVirtualAddress();

	handle.ptr += handleIncrement;
	m_D3DObjects.device->CreateConstantBufferView(&viewCBDesc, handle);

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
	vertexSrvDesc.Buffer.NumElements = (static_cast<UINT>(model.vertices.size()) * sizeof(UINT)) / sizeof(float);
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
	m_D3DObjects.device->CreateShaderResourceView(m_D3DResources.materialCB, &textureSrvDesc, handle);
}