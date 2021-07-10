#include "Graphics.h"


using namespace DirectX;

#define ALIGN(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)
#define SAFE_RELEASE(x) { if (x) { x->Release(); x = NULL; } }

Graphics::Graphics(Config config)
	:
	m_D3DParams(config.width, config.height, true),
	m_Gravity(Vector3(0.0, -2.0, 0.0)),
	resolver(maxContacts * 8),
	physicsDemo(false)
{
	cData.contactArray = contacts;
	gt = GameTimer();
	gt.Start();
}

Graphics::~Graphics()
{
	gt.Stop();
	Shutdown();
}

void Graphics::Init(HWND hwnd)
{
	m_Geometry = make_unique<MeshGeometry>();
	std::string name = "Geometry";
	m_Geometry->name = name;
	m_Geometries[m_Geometry->name] = std::move(m_Geometry);
	
	if (physicsDemo)
	{
		LoadModel("models/DefaultCube.obj", name, 0);
		LoadModel("models/Plane.obj", name, 1);

		m_Gravity = Gravity(gravityAmount);

		cubeBody.body = new RigidBody;
		cubeBody.body->SetPosition(Vector3(0, 4, 0));
		cubeBody.body->SetMass(1.0f);
		cubeBody.halfSize = Vector3(0.1, 0.1, 0.1);
		Matrix3 tensor;
		//	tensor.setBlockInertiaTensor(Vector3(5, 5, 5), 10.f);
		//	cubeBody.body->SetInertiaTensor(tensor);
		cubeBody.body->SetLinearDamping(0.95f);
		cubeBody.body->SetAngularDamping(0.8f);
		cubeBody.body->ClearAccumulators();
		cubeBody.body->SetAcceleration(Vector3::GRAVITY);
		cubeBody.body->SetCanSleep(false);
		cubeBody.body->SetAwakeStatus();
		cubeBody.body->CalculateDerivedData();
		cubeBody.CalculateInternals();


		registry.add(cubeBody.body, &m_Gravity);

	}
	else
	{
		LoadModel("models/Portals.obj", name, 0);
		LoadModel("models/Ground.obj", name, 1);
		LoadSkinnedModel();
	}

	InitializeShaderCompiler();

	CreateDevice();
	CreateCommandQueue();
	CreateCommandAllocator();
	CreateFence();
	CreateSwapChain(hwnd);
	CreateCommandList();
	ResetCommandList();

	CreateDSVDescriptorHeap();
	CreateRTVDescriptorHeaps();

	SeedRandomVector(m_RandomVectorSeed0);
	SeedRandomVector(m_RandomVectorSeed1);

	CreateGBufferPassCommandAllocator();
	CreateGBufferPassCommandList();
	ResetGBufferPassCommandList();
	CompileGBufferPassShaders();
	CreateGBufferPassRootSignature();
	CreateGBufferPassPSO();
	CreateGBufferPassRTVDescriptorHeaps();
	CreateGBufferPassRTVResources();
	CreateGBufferPassRTVs();
	CreateDepthStencilView();

	CreateBackBufferRTV();

	BuildMeshGeometry("Geometry");
	BuildRenderItems();
	BuildFrameResources();
	//	CreateTexture(m_Material);

	CreateBottomLevelAS(m_RayTracingPassRenderItems[0], m_RayTracingPassRenderItems[1], 0u);
//	CreateBottomLevelAS(m_RayTracingPassRenderItems[1], 1u);
	CreateTopLevelAS();
	CreateDXROutput();
	CreateDescriptorHeaps();

	CreateRayGenProgram();
	CreateMissProgram();
	CreateClosestHitProgram();
//	CreateShadowHitProgram();
	CreatePipelineStateObject();
	CreateShaderTable();

	m_D3DObjects.gBufferPassCommandList->Close();

	m_D3DObjects.commandList->Close();
	ID3D12CommandList* pGraphicsList[2] = { m_D3DObjects.commandList, m_D3DObjects.gBufferPassCommandList };
	m_D3DObjects.commandQueue->ExecuteCommandLists(2, pGraphicsList);
	
	WaitForGPU();
	ResetCommandList();
	ResetGBufferCommandList();
}

void Graphics::Update()
{
	gt.Tick();
	m_CurrFrameResourceIndex = (m_CurrFrameResourceIndex + 1) % gNumFrameResources;
	m_CurrFrameResource = m_FrameResources[m_CurrFrameResourceIndex].get();

	// Check if GPU has finished processing commands of the current frame resource
	// If not, wait until the GPU has completed commands up to this fence point

	if (m_CurrFrameResource->Fence != 0 && m_D3DObjects.fence->GetCompletedValue() < m_CurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		HRESULT hr = m_D3DObjects.fence->SetEventOnCompletion(m_CurrFrameResource->Fence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	if (physicsDemo)
	{
	//	float duration = (float)TimingData::get().lastFrameDuration * 0.001f;
	//	if (duration <= 0.0f) return;

		float duration = 0.03f;

		// Start with no forces or acceleration
		cubeBody.body->ClearAccumulators();

		// Add the forces acting on the cube
		registry.updateForces(duration);

	//	cubeBody.body->AddForce(gravityAmount);

		// Update the cubes physics
	//	cubeBody.body->Integrate(duration);

		// Update the objects
		updateObjects(duration);

		// Perform the contact generation
		generateContacts();

		// Resolve the detected contacts
		resolver.ResolveContacts(cData.contactArray, cData.contactCount, duration);
	}

	UpdateLightsSceneCB();
	UpdateObjectCBs();
	UpdateSkinnedCBs(gt);
	UpdateMaterialCBs();
	UpdateGBufferPassSceneCB();
	UpdateRayTracingPassSceneCB();
}

void Graphics::Render()
{
	BuildGBufferCommandList();
	BuildCommandList();
	Present();
	MoveToNextFrame();
	ResetGBufferCommandList();
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


#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#include "stb_image.h"
#endif

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#endif

void Graphics::Validate(HRESULT hr, LPWSTR message)
{
	if (FAILED(hr))
	{
		MessageBox(NULL, message, L"Error", MB_OK);
		PostQuitMessage(EXIT_FAILURE);
	}
}

void Graphics::LoadModel(std::string filename, std::string geometryName, INT matCBIndex)
{
	auto memory = malloc(sizeof(unique_ptr<Model>));

	auto model = new (memory) unique_ptr<Model>();
	*model = std::make_unique<Model>();
	auto material = std::make_unique<Material>();

	using namespace tinyobj;
	attrib_t attrib;
	std::vector<shape_t> shapes;
	std::vector<material_t> materials;
	std::string err;

	if (!LoadObj(&attrib, &shapes, &materials, &err, filename.c_str(), "materials\\"))
	{
		throw std::runtime_error(err);
	}

	material->name = materials[0].name;
	material->materialCBIndex = matCBIndex;
	material->ambient = XMFLOAT3(materials[0].ambient[0], materials[0].ambient[1], materials[0].ambient[2]);
	material->diffuse = XMFLOAT3(materials[0].diffuse[0], materials[0].diffuse[1], materials[0].diffuse[2]);
	material->specular = XMFLOAT3(materials[0].specular[0], materials[0].specular[1], materials[0].specular[2]);
	material->transmittance = XMFLOAT3(materials[0].transmittance[0], materials[0].transmittance[1], materials[0].transmittance[2]);
	material->emission = XMFLOAT3(materials[0].emission[0], materials[0].emission[1], materials[0].emission[2]);
	material->shininess = materials[0].shininess;
	material->ior = materials[0].ior;
	material->dissolve = materials[0].dissolve;
	material->roughness = materials[0].roughness;
	material->metallic = materials[0].metallic;
	material->sheen = materials[0].sheen;
	material->texturePath = materials[0].diffuse_texname;

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

			vertex.normal =
			{
				attrib.normals[3 * index.normal_index + 2],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 0]
			};

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(model->get()->vertices.size());
				model->get()->vertices.push_back(vertex);
			}

			model->get()->indices.push_back(uniqueVertices[vertex]);
		};
	}

	unique_ptr<SubmeshGeometry> submeshGeo = make_unique<SubmeshGeometry>();

	submeshGeo->indexCount = model->get()->indices.size();
	submeshGeo->startIndexLocation = m_D3DValues.indicesCount;
	submeshGeo->vertexCount = (UINT)model->get()->vertices.size();
	submeshGeo->baseVertexLocation = m_D3DValues.vertexCount;

	m_D3DValues.vertexCount += (UINT)model->get()->vertices.size();
	m_D3DValues.indicesCount += (UINT)model->get()->indices.size();

	m_Models[filename] = std::move(*model);
	m_Materials[filename] = std::move(material);
	
	m_Geometries[geometryName]->drawArgs[filename] = std::move(submeshGeo);
}

void Graphics::FormatTexture(TextureInfo& info, UINT8* pixels)
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

TextureInfo Graphics::LoadTexture(std::string filepath)
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

void Graphics::InitializeShaderCompiler()
{
	HRESULT hr = m_ShaderCompilerInfo.DxcDllHelper.Initialize();
	Validate(hr, L"Failed to initialize DxCDllSupport!");

	hr = m_ShaderCompilerInfo.DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &m_ShaderCompilerInfo.compiler);
	Validate(hr, L"Failed to create DxcCompiler!");

	hr = m_ShaderCompilerInfo.DxcDllHelper.CreateInstance(CLSID_DxcLibrary, &m_ShaderCompilerInfo.library);
	Validate(hr, L"Failed to create DxcLibrary!");
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
	Validate(hr, L"Failed to create factory!");

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != m_D3DObjects.factory->EnumAdapters1(adapterIndex, &m_D3DObjects.adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 adapterDesc = {};
		m_D3DObjects.adapter->GetDesc1(&adapterDesc);

		if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(m_D3DObjects.adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_D3DObjects.device))))
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
			Validate(E_FAIL, L"Failed to create ray tracing device!");
		}
	}
}

void Graphics::CreateCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT hr = m_D3DObjects.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_D3DObjects.commandQueue));
	Validate(hr, L"Failed to create command queue!");

#if NAME_D3D_RESOURCES
	m_D3DObjects.commandQueue->SetName(L"D3D12 Command Queue");
#endif
}

void Graphics::CreateCommandAllocator()
{
	for (int n = 0; n < 2; ++n)
	{
		HRESULT hr = m_D3DObjects.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_D3DObjects.commandAllocators[n]));
		Validate(hr, L"Failed to create command allocator!");
	}

#if NAME_D3D_RESOURCES
	if (n = 0) m_D3DObjects.commandAllocators[n]->SetName(L"D3D12 Command Allocator 0");
	else m_D3DObjects.commandAllocators[n]->SetName(L"D3D12 Command Allocator 1");
#endif
}

void Graphics::CreateGBufferPassCommandAllocator()
{
	for (int n = 0; n < 2; ++n)
	{
		HRESULT hr = m_D3DObjects.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_D3DObjects.gBufferPassCommandAllocators[n]));
		Validate(hr, L"Failed to create raster command allocator!");
	}

#if NAME_D3D_RESOURCES
	if (n = 0) m_D3DObjects.commandAllocators[n]->SetName(L"D3D12 Raster Command Allocator 0");
	else m_D3DObjects.commandAllocators[n]->SetName(L"D3D12 Raster Command Allocator 1");
#endif
}

void Graphics::CreateFence()
{
	HRESULT hr = m_D3DObjects.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_D3DObjects.fence));
	Validate(hr, L"Failed to create fence!");

#if NAME_D3D_RESOURCES
	m_D3DObjects.fence->SetName(L"D3D12 Fence");
#endif

	m_D3DValues.fenceValues[m_D3DValues.frameIndex]++;

	m_D3DObjects.fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);

	if (m_D3DObjects.fenceEvent == nullptr)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		Validate(hr, L"Failed to create fence event!");
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
	Validate(hr, L"Failed to create swap chain!");

	hr = m_D3DObjects.factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	Validate(hr, L"Failed to make window association");

	hr = swapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&m_D3DObjects.swapChain));
	Validate(hr, L"Failed to cast swap chain!");

	swapChain->Release();
	m_D3DValues.frameIndex = m_D3DObjects.swapChain->GetCurrentBackBufferIndex();
}

void Graphics::CreateGBufferPassCommandList()
{
	HRESULT hr = m_D3DObjects.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_D3DObjects.gBufferPassCommandAllocators[m_D3DValues.frameIndex], nullptr, IID_PPV_ARGS(&m_D3DObjects.gBufferPassCommandList));
	Validate(hr, L"Failed to create raster command list!");
	hr = m_D3DObjects.gBufferPassCommandList->Close();

#if NAME_D3D_RESOURCES
	m_D3DObjects.commandList->SetName(L"D3D12 Raster Command List");
#endif
}

void Graphics::ResetGBufferPassCommandList()
{
	HRESULT hr = m_D3DObjects.gBufferPassCommandAllocators[m_D3DValues.frameIndex]->Reset();
	Validate(hr, L"Failed to reset G Buffer Command Allocator!");

	hr = m_D3DObjects.gBufferPassCommandList->Reset(m_D3DObjects.gBufferPassCommandAllocators[m_D3DValues.frameIndex], nullptr);
	Validate(hr, L"Failed to reset G Buffer Command List!");
}


void Graphics::CreateDSVDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC dsvDescHeapDesc = {};
	dsvDescHeapDesc.NumDescriptors = 1;
	dsvDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDescHeapDesc.NodeMask = 0;

	HRESULT hr = m_D3DObjects.device->CreateDescriptorHeap(&dsvDescHeapDesc, IID_PPV_ARGS(&m_D3DResources.dsvDescriptorHeap));
	Validate(hr, L"Failed to create Depth Stencil View Heap!");
}

void Graphics::CreateGBufferPassRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsConstantBufferView(2);

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = _countof(slotRootParameter);
	rootSigDesc.pParameters = slotRootParameter;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ID3DBlob* signature;
	ID3DBlob* error;

	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
	Validate(hr, L"Failed to serialize root signature!");

	hr = m_D3DObjects.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_D3DObjects.gBufferPassRootSignature));
	Validate(hr, L"Failed to create root signature!");
}

void Graphics::CreateDepthStencilView()
{
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0.f;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 0u;
	heapProps.VisibleNodeMask = 0u;

	D3D12_RESOURCE_DESC dsvResDesc = {};
	dsvResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsvResDesc.DepthOrArraySize = 1;
	dsvResDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvResDesc.Width = m_D3DParams.width;
	dsvResDesc.Height = m_D3DParams.height;
	dsvResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	dsvResDesc.MipLevels = 0;
	dsvResDesc.SampleDesc.Count = 1;
	dsvResDesc.SampleDesc.Quality = 0;

	HRESULT hr = m_D3DObjects.device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_D3DParams.width, m_D3DParams.height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&m_D3DResources.depthStencilView)
	);

	Validate(hr, L"Failed to create Depth Stencil View!");
	hr = m_D3DObjects.device->GetDeviceRemovedReason();
	Validate(hr, L"Device Removed");

#if NAME_D3D_RESOURCES
	m_D3DResources.depthStencilView->SetName(L"D3D12 Depth Stencil View");
#endif

	m_D3DObjects.device->CreateDepthStencilView(m_D3DResources.depthStencilView, &dsvDesc, m_D3DResources.dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void Graphics::CompileGBufferPassShaders()
{
#if defined (DEBUG)
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	HRESULT hr = D3DCompileFromFile(L"shaders/VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &m_D3DObjects.vsBlob, nullptr);
	Validate(hr, L"Failed to compile Vertex Shader!");
	hr = D3DCompileFromFile(L"shaders/PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &m_D3DObjects.psBlob, nullptr);
	Validate(hr, L"Failed to compile Pixel Shader!");
}

void Graphics::CreateCommandList()
{
	HRESULT hr = m_D3DObjects.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_D3DObjects.commandAllocators[m_D3DValues.frameIndex], nullptr, IID_PPV_ARGS(&m_D3DObjects.commandList));
	hr = m_D3DObjects.commandList->Close();
	Validate(hr, L"Failed to create command list!");

#if NAME_D3D_RESOURCES
	m_D3DObjects.commandList->SetName(L"D3D12 Command List");
#endif
}

void Graphics::CreateGBufferPassPSO()
{
	D3D12_INPUT_ELEMENT_DESC ieDesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		
	};

	D3D12_DEPTH_STENCIL_DESC desc;
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	desc.StencilEnable = FALSE;
	desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { ieDesc, _countof(ieDesc) };
	psoDesc.pRootSignature = m_D3DObjects.gBufferPassRootSignature;

	psoDesc.VS = { reinterpret_cast<UINT8*>(m_D3DObjects.vsBlob->GetBufferPointer()), m_D3DObjects.vsBlob->GetBufferSize() };
	psoDesc.PS = { reinterpret_cast<UINT8*>(m_D3DObjects.psBlob->GetBufferPointer()), m_D3DObjects.psBlob->GetBufferSize() };

	CD3DX12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerDesc.FrontCounterClockwise = TRUE;

	psoDesc.DepthStencilState = desc;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 4;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	/*
	psoDesc.RTVFormats[4] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[5] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[6] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[7] = DXGI_FORMAT_R8G8B8A8_UNORM;
	*/

	HRESULT hr = m_D3DObjects.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_D3DObjects.gBufferPassPipelineState));
	Validate(hr, L"Failed to create G Buffer Pipeline State!");
}

void Graphics::CreateGBufferPassRTVDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC gBufRTVHeapDesc = {};
	gBufRTVHeapDesc.NumDescriptors = 4;
	gBufRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	HRESULT hr = m_D3DObjects.device->CreateDescriptorHeap(&gBufRTVHeapDesc, IID_PPV_ARGS(&m_D3DResources.gBufferPassRTVHeap));
	Validate(hr, L"Failed to create G Buffer RTV Descriptor Heap");
}

void Graphics::CreateGBufferPassRTVResources()
{
	D3D12_RESOURCE_DESC rtvDesc = {};
	rtvDesc.Width = m_D3DParams.width;
	rtvDesc.Height = m_D3DParams.height;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.DepthOrArraySize = 1;
	rtvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
	rtvDesc.MipLevels = 1;
	rtvDesc.SampleDesc.Count = 1;
	rtvDesc.SampleDesc.Quality = 0;
	rtvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	FLOAT colour[4] = { 0.22f, 0.33f, 0.44f, 0.f };

	D3D12_CLEAR_VALUE rtvClearCol = {};
	rtvClearCol.Color[0] = colour[0];
	rtvClearCol.Color[1] = colour[1];
	rtvClearCol.Color[2] = colour[2];
	rtvClearCol.Color[3] = colour[3];
	rtvClearCol.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	for (int i = 0; i < 1; i++)
	{
		HRESULT hr = m_D3DObjects.device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rtvDesc,
			D3D12_RESOURCE_STATE_PRESENT,
			&rtvClearCol,
			IID_PPV_ARGS(&m_D3DResources.gBufferWorldPos));

		hr = m_D3DObjects.device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rtvDesc,
			D3D12_RESOURCE_STATE_PRESENT,
			&rtvClearCol,
			IID_PPV_ARGS(&m_D3DResources.gBufferNormal));

		hr = m_D3DObjects.device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rtvDesc,
			D3D12_RESOURCE_STATE_PRESENT,
			&rtvClearCol,
			IID_PPV_ARGS(&m_D3DResources.gBufferDiffuse));

		hr = m_D3DObjects.device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rtvDesc,
			D3D12_RESOURCE_STATE_PRESENT,
			&rtvClearCol,
			IID_PPV_ARGS(&m_D3DResources.gBufferSpecular));
	}

	/*
	D3D12_RESOURCE_DESC gBufSrvResourceDesc = {};
	gBufSrvResourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	gBufSrvResourceDesc.Width = m_D3DParams.width;
	gBufSrvResourceDesc.Height = m_D3DParams.height;
	gBufSrvResourceDesc.MipLevels = 1;
	gBufSrvResourceDesc.DepthOrArraySize = 1;
	gBufSrvResourceDesc.SampleDesc.Count = 1;
	gBufSrvResourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	gBufSrvResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	

	for (int i = 0; i < 2; i++)
	{
		HRESULT hr = m_D3DObjects.device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&gBufSrvResourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_D3DResources.gBufSRVWorldPos[i]));

		hr = m_D3DObjects.device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&gBufSrvResourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_D3DResources.gBufSRVNormal[i]));

		hr = m_D3DObjects.device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&gBufSrvResourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_D3DResources.gBufSRVDiffuse[i]));

		hr = m_D3DObjects.device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&gBufSrvResourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_D3DResources.gBufSRVSpecular[i]));
	}
	*/
}

void Graphics::CreateGBufferPassRTVs()
{
	HRESULT hr;
	D3D12_CPU_DESCRIPTOR_HANDLE gBufRTVHandle = m_D3DResources.gBufferPassRTVHeap->GetCPUDescriptorHandleForHeapStart();

	for (int i = 0; i < 1; i++)
	{
		m_D3DObjects.device->CreateRenderTargetView(m_D3DResources.gBufferWorldPos, nullptr, gBufRTVHandle);

		gBufRTVHandle.ptr += m_D3DValues.rtvDescSize;

		m_D3DObjects.device->CreateRenderTargetView(m_D3DResources.gBufferNormal, nullptr, gBufRTVHandle);

		gBufRTVHandle.ptr += m_D3DValues.rtvDescSize;

		m_D3DObjects.device->CreateRenderTargetView(m_D3DResources.gBufferDiffuse, nullptr, gBufRTVHandle);

		gBufRTVHandle.ptr += m_D3DValues.rtvDescSize;

		m_D3DObjects.device->CreateRenderTargetView(m_D3DResources.gBufferSpecular, nullptr, gBufRTVHandle);

		if (i < 1)
		{
			gBufRTVHandle.ptr += m_D3DValues.rtvDescSize;
		}

	}

#if NAME_D3D_RESOURCES
	m_D3DResources.gBufferWorldPos->SetName(L"World Position G Buffer");
	m_D3DResources.gBufferNormal->SetName(L"World Normals G Buffer");
	m_D3DResources.gBufferDiffuse->SetName(L"Diffuse G Buffer");
	m_D3DResources.gBufferSpecular->SetName(L"Specular G Buffer");
#endif

}


void Graphics::ResetCommandList()
{
	HRESULT hr = m_D3DObjects.commandAllocators[m_D3DValues.frameIndex]->Reset();
	Validate(hr, L"Failed to reset command allocator!");

	hr = m_D3DObjects.commandList->Reset(m_D3DObjects.commandAllocators[m_D3DValues.frameIndex], nullptr);
	Validate(hr, L"Failed to reset command list!");
}

void Graphics::ResetGBufferCommandList()
{
	HRESULT hr = m_D3DObjects.gBufferPassCommandAllocators[m_D3DValues.frameIndex]->Reset();
	Validate(hr, L"Failed to reset G Buffer Command Allocator!");


	hr = m_D3DObjects.gBufferPassCommandList->Reset(m_D3DObjects.gBufferPassCommandAllocators[m_D3DValues.frameIndex], m_D3DObjects.gBufferPassPipelineState);
	Validate(hr, L"Failed to reset G Buffer Command List!");
}

void Graphics::CreateRTVDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = m_D3DValues.swapChainBufferCount;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = m_D3DObjects.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_D3DResources.rtvHeap));
	Validate(hr, L"Failed to create RTV descriptor heap!");

#if NAME_D3D_RESOURCES
	m_D3DResources.rtvHeap->SetName(L"Render Target View Descriptor Heap");
#endif

	m_D3DValues.rtvDescSize = m_D3DObjects.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void Graphics::CreateBackBufferRTV()
{
	HRESULT hr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_D3DResources.rtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (UINT n = 0; n < m_D3DValues.swapChainBufferCount; ++n)
	{
		hr = m_D3DObjects.swapChain->GetBuffer(n, IID_PPV_ARGS(&m_D3DObjects.backBuffer[n]));
		Validate(hr, L"Failed to create back buffers!");

		m_D3DObjects.device->CreateRenderTargetView(m_D3DObjects.backBuffer[n], nullptr, rtvHandle);

#if NAME_D3D_RESOURCES
		if (n == 0) m_D3DObjects.backBuffer[0]->SetName(L"Back Buffer 0");
		m_D3DObjects.backBuffer[1]->SetName(L"Back Buffer 1");
#endif

		rtvHandle.ptr += m_D3DValues.rtvDescSize;

	}
}

void Graphics::BuildMeshGeometry(std::string geometryName)
{
	/*
		Build Mesh Data
			- Change Load Model to return MeshData object
			- Save as mesh data variables
		Build Submesh Geometry for each mesh data object
		Build Geometry for each mesh data object
		Delete old create buffers
	*/
	std::vector<Vertex> vertices(m_D3DValues.vertexCount);
	std::vector<std::uint32_t> indices;

	UINT x = 0;

	for (auto it = std::begin(m_Models); it != std::end(m_Models); ++it)
	{
		auto tempModel = *it->second;

		for (size_t i = 0; i < tempModel.vertices.size(); i++)
		{
			vertices[x].position = tempModel.vertices[i].position;
			vertices[x].normal = tempModel.vertices[i].normal;
			vertices[x].uv = tempModel.vertices[i].uv;
			x++;
		}

		auto modelIndices = tempModel.indices;
		indices.insert(indices.end(), std::begin(modelIndices), std::end(modelIndices));

	}
	const UINT vertexBufferByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT indexBufferByteSize = (UINT)indices.size() * sizeof(uint32_t);

	HRESULT hr = D3DCreateBlob(vertexBufferByteSize, &m_Geometries[geometryName]->vertexBufferCPU);
	Validate(hr, L"Failed to create Vertex Buffer Blob!");
	CopyMemory(m_Geometries[geometryName]->vertexBufferCPU->GetBufferPointer(), vertices.data(), vertexBufferByteSize);

	hr = D3DCreateBlob(indexBufferByteSize, &m_Geometries[geometryName]->indexBufferCPU);
	Validate(hr, L"Failed to create Index Buffer Blob!");
	CopyMemory(m_Geometries[geometryName]->indexBufferCPU->GetBufferPointer(), indices.data(), indexBufferByteSize);

	m_Geometries[geometryName]->vertexByteStride = sizeof(Vertex);
	m_Geometries[geometryName]->vertexBufferByteSize = vertexBufferByteSize;
	m_Geometries[geometryName]->indexBufferFormat = DXGI_FORMAT_R32_UINT;
	m_Geometries[geometryName]->indexBufferByteSize = indexBufferByteSize;

	D3D12BufferCreateInfo vertexBufferCreateInfo = {};
	vertexBufferCreateInfo.size = vertexBufferByteSize;

	CreateVertexBuffer(geometryName);

	D3D12BufferCreateInfo indexBufferCreateInfo = {};
	indexBufferCreateInfo.size = indexBufferByteSize;

	CreateIndexBuffer(geometryName);
}

ID3D12Resource* Graphics::CreateDefaultBuffer(const void* initData, ID3D12Resource* uploadBuffer, D3D12BufferCreateInfo bufferCreateInfo)
{
	ID3D12Resource* defaultBuffer;

	HRESULT hr = m_D3DObjects.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(bufferCreateInfo.defaultBufferHeapType),
		bufferCreateInfo.defaultBufferHeapFlags,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferCreateInfo.size),
		bufferCreateInfo.defaultBufferState,
		nullptr,
		IID_PPV_ARGS(&defaultBuffer)
	);

	Validate(hr, L"Failed to create default buffer!");

	hr = m_D3DObjects.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(bufferCreateInfo.uploadBufferHeapType),
		bufferCreateInfo.uploadBufferFlags,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferCreateInfo.size),
		bufferCreateInfo.uploadBufferState,
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)
	);

	Validate(hr, L"Failed to create upload buffer!");

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = initData;
	subresourceData.RowPitch = bufferCreateInfo.size;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	m_D3DObjects.commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		defaultBuffer,
		bufferCreateInfo.defaultBufferState,
		D3D12_RESOURCE_STATE_COPY_DEST
	));

	UpdateSubresources<1>(m_D3DObjects.commandList, defaultBuffer, uploadBuffer, 0, 0, 1, &subresourceData);

	//	memcpy(defaultBuffer, uploadBuffer, bufferCreateInfo.size);

	m_D3DObjects.commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		defaultBuffer,
		D3D12_RESOURCE_STATE_COPY_DEST,
		bufferCreateInfo.defaultBufferFinalState
	));

	return defaultBuffer;
}

void Graphics::BuildRenderItems()
{
	auto skull = std::make_unique<RenderItem>();
	auto altar = std::make_unique<RenderItem>();
	auto skinnedSoldier = std::make_unique<RenderItem>();

	skull->name = "skull";
	skull->world = XMMatrixRotationY(XMConvertToRadians(90.0f)) * XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	XMStoreFloat3x4(&skull->world3x4, XMMatrixTranspose(skull->world));
	skull->objCBIndex = 0;
	skull->matCBIndex = 0;
	skull->geometry = m_Geometries["Geometry"].get();
	skull->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	if (physicsDemo)
	{
		skull->indexCount = skull->geometry->drawArgs["models/DefaultCube.obj"].get()->indexCount;
		skull->vertexCount = skull->geometry->drawArgs["models/DefaultCube.obj"].get()->vertexCount;
		skull->startIndexLocation = skull->geometry->drawArgs["models/DefaultCube.obj"].get()->startIndexLocation;
		skull->baseVertexLocation = skull->geometry->drawArgs["models/DefaultCube.obj"].get()->baseVertexLocation;
	}
	else
	{
		skull->indexCount = skull->geometry->drawArgs["models/Portals.obj"].get()->indexCount;
		skull->vertexCount = skull->geometry->drawArgs["models/Portals.obj"].get()->vertexCount;
		skull->startIndexLocation = skull->geometry->drawArgs["models/Portals.obj"].get()->startIndexLocation;
		skull->baseVertexLocation = skull->geometry->drawArgs["models/Portals.obj"].get()->baseVertexLocation;
	}

	m_GBufferPassRenderItems.push_back(skull.get());
	m_RayTracingPassRenderItems.push_back(skull.get());
	m_AllRenderItems.push_back(std::move(skull));

	altar->name = "altar";
	altar->world = XMMatrixScaling(1.f, 1.f, 1.f) * XMMatrixRotationY(XMConvertToRadians(90.0f)) * XMMatrixTranslation(0.0, 0.f, 0.0f);
	XMStoreFloat3x4(&altar->world3x4, XMMatrixTranspose(altar->world));
	altar->objCBIndex = 1;
	altar->matCBIndex = 1;
	altar->geometry = m_Geometries["Geometry"].get();
	altar->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	
	if (physicsDemo)
	{
		altar->indexCount = altar->geometry->drawArgs["models/Plane.obj"].get()->indexCount;
		altar->vertexCount = altar->geometry->drawArgs["models/Plane.obj"].get()->vertexCount;
		altar->startIndexLocation = altar->geometry->drawArgs["models/Plane.obj"].get()->startIndexLocation;
		altar->baseVertexLocation = altar->geometry->drawArgs["models/Plane.obj"].get()->baseVertexLocation;
	}
	else
	{
		altar->indexCount = altar->geometry->drawArgs["models/Ground.obj"].get()->indexCount;
		altar->vertexCount = altar->geometry->drawArgs["models/Ground.obj"].get()->vertexCount;
		altar->startIndexLocation = altar->geometry->drawArgs["models/Ground.obj"].get()->startIndexLocation;
		altar->baseVertexLocation = altar->geometry->drawArgs["models/Ground.obj"].get()->baseVertexLocation;
	}

	m_GBufferPassRenderItems.push_back(altar.get());
	m_RayTracingPassRenderItems.push_back(altar.get());
	m_AllRenderItems.push_back(std::move(altar));

	if (!physicsDemo)
	{
		skinnedSoldier->name = "soldier";
		skinnedSoldier->world = XMMatrixScaling(1.f, 1.f, 1.f) * XMMatrixRotationY(XMConvertToRadians(-90.0f)) * XMMatrixTranslation(0.0, 0.f, -3.0f);
		XMStoreFloat3x4(&skinnedSoldier->world3x4, XMMatrixTranspose(skinnedSoldier->world));
		skinnedSoldier->objCBIndex = 2;
		skinnedSoldier->matCBIndex = 2;
		skinnedSoldier->geometry = m_Geometries["Geometry"].get();
		skinnedSoldier->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		for (UINT i = 0; i < m_SkinnedMats.size(); ++i)
		{
			string submeshName = "sm_" + to_string(i);
			skinnedSoldier->indexCount = skinnedSoldier->geometry->drawArgs[submeshName].get()->indexCount;
			skinnedSoldier->vertexCount = skinnedSoldier->geometry->drawArgs[submeshName].get()->vertexCount;
			skinnedSoldier->startIndexLocation = skinnedSoldier->geometry->drawArgs[submeshName].get()->startIndexLocation;
			skinnedSoldier->baseVertexLocation = skinnedSoldier->geometry->drawArgs[submeshName].get()->baseVertexLocation;
		}
		skinnedSoldier->SkinnedCBIndex = 0;
		skinnedSoldier->SkinnedModelInst = m_SkinnedModelInst.get();

		m_GBufferPassRenderItems.push_back(skinnedSoldier.get());
		m_RayTracingPassRenderItems.push_back(skinnedSoldier.get());
		m_AllRenderItems.push_back(std::move(skinnedSoldier));

	}
}

void Graphics::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; i++)
	{
		m_FrameResources.push_back(std::make_unique<FrameResource>(m_D3DObjects.device, 2, (UINT)m_AllRenderItems.size() - 1, 1, (UINT)m_Materials.size()));
	}
}

void Graphics::DrawRenderItems(const std::vector<RenderItem*>& renderItems)
{
	UINT objectCBByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(ObjectCB));
	UINT materialCBByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(MaterialCB));
	UINT gBufferPassCBByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(GBufferPassSceneCB));
	UINT skinnedCBByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(SkinnedCB));

	auto objectCB = m_CurrFrameResource->objectCB->Resource();
	auto materialCB = m_CurrFrameResource->materialCB->Resource();
	auto gBufferPassCB = m_CurrFrameResource->gBufferPassSceneCB->Resource();
	auto skinnedCB = m_CurrFrameResource->skinnedCB->Resource();

	for (size_t i = 0; i < renderItems.size(); i++)
	{
		auto renderItem = renderItems[i];

		m_D3DObjects.gBufferPassCommandList->IASetVertexBuffers(0, 1, &renderItem->geometry->VertexBufferView());
		m_D3DObjects.gBufferPassCommandList->IASetIndexBuffer(&renderItem->geometry->IndexBufferView());
		m_D3DObjects.gBufferPassCommandList->IASetPrimitiveTopology(renderItem->primitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objectCBAddress = objectCB->GetGPUVirtualAddress() + (objectCBByteSize * renderItem->objCBIndex);
		D3D12_GPU_VIRTUAL_ADDRESS materialCBAddress = materialCB->GetGPUVirtualAddress() + (materialCBByteSize * renderItem->matCBIndex);

		m_D3DObjects.gBufferPassCommandList->SetGraphicsRootConstantBufferView(0, objectCBAddress);
		m_D3DObjects.gBufferPassCommandList->SetGraphicsRootConstantBufferView(1, materialCBAddress);

		D3D12_GPU_VIRTUAL_ADDRESS gBufferPassCBAddress = gBufferPassCB->GetGPUVirtualAddress();

		m_D3DObjects.gBufferPassCommandList->SetGraphicsRootConstantBufferView(2, gBufferPassCBAddress);

		if (renderItem->SkinnedModelInst != nullptr)
		{
			D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + renderItem->SkinnedCBIndex * skinnedCBByteSize;
			m_D3DObjects.gBufferPassCommandList->SetGraphicsRootConstantBufferView(3, skinnedCBAddress);
		}
		else
		{
			m_D3DObjects.gBufferPassCommandList->SetGraphicsRootConstantBufferView(3, 0);
		}

		m_D3DObjects.gBufferPassCommandList->DrawIndexedInstanced(renderItem->indexCount, 1, renderItem->startIndexLocation, renderItem->baseVertexLocation, 0);
	}
};

void Graphics::CreateBuffer(D3D12BufferInfo& info, ID3D12Resource** ppResource)
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
	Validate(hr, L"Failed to create buffer resource!");
}

void Graphics::CreateVertexBuffer(std::string geometryName)
{
	ID3D12Resource* tempVertexBuffer;
	D3D12BufferInfo info(m_Geometries[geometryName].get()->vertexBufferByteSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	info.flags = D3D12_RESOURCE_FLAG_NONE;
	CreateBuffer(info, &tempVertexBuffer);

#if NAME_D3D_RESOURCES
	m_D3DResources.vertexBuffer->SetName(L"Vertex Buffer");
#endif

	UINT8* pVertexDataBegin;
	D3D12_RANGE readRange = {};
	HRESULT hr = tempVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
	Validate(hr, L"Failed to map vertex buffer");

	memcpy(pVertexDataBegin, m_Geometries[geometryName].get()->vertexBufferCPU->GetBufferPointer(), info.size);
	tempVertexBuffer->Unmap(0, nullptr);

	m_Geometries[geometryName].get()->vertexBufferGPU = tempVertexBuffer;

	/*
	m_D3DResources.vertexBufferView.BufferLocation = m_D3DResources.vertexBuffer->GetGPUVirtualAddress();
	m_D3DResources.vertexBufferView.SizeInBytes = static_cast<UINT>(info.size);
	m_D3DResources.vertexBufferView.StrideInBytes = sizeof(Vertex);
	*/
	
}

void Graphics::CreateIndexBuffer(std::string geometryName)
{
	ID3D12Resource* tempIndexBuffer;

	D3D12BufferInfo info(m_Geometries[geometryName].get()->indexBufferByteSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	CreateBuffer(info, &tempIndexBuffer);

#if NAME_D3D_RESOURCES
	m_D3DResources.indexBuffer->SetName(L"Index Buffer");
#endif

	UINT8* pIndexDataBegin;
	D3D12_RANGE readRange = {};
	HRESULT hr = tempIndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
	Validate(hr, L"Failed to map index buffer");

	memcpy(pIndexDataBegin, m_Geometries[geometryName].get()->indexBufferCPU->GetBufferPointer(), info.size);
	tempIndexBuffer->Unmap(0, nullptr);

	m_Geometries[geometryName].get()->indexBufferGPU = tempIndexBuffer;
	/*
	m_D3DResources.indexBufferView.BufferLocation = m_D3DResources.indexBuffer->GetGPUVirtualAddress();
	m_D3DResources.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_D3DResources.indexBufferView.SizeInBytes = static_cast<UINT>(info.size);
	*/
}

void Graphics::CreateTexture(Material& material)
{
	TextureInfo texture = LoadTexture(material.texturePath);
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
	Validate(hr, L"Failed to create texture!");

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
	Validate(hr, L"Failed to create texture upload heap!");

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

void Graphics::UpdateObjectCBs()
{
	auto currentObjectCB = m_CurrFrameResource->objectCB.get();

	for (auto& renderItem : m_AllRenderItems)
	{
			
		if (physicsDemo)
		{

			if (renderItem->name == "skull")
			{
				Matrix4 skullWorldMat = cubeBody.body->GetTransform();
				float skullWorld0 = skullWorldMat.data[0];
				float skullWorld1 = skullWorldMat.data[1];
				float skullWorld2 = skullWorldMat.data[2];
				float skullWorld3 = skullWorldMat.data[3];

				float skullWorld4 = skullWorldMat.data[4];
				float skullWorld5 = skullWorldMat.data[5];
				float skullWorld6 = skullWorldMat.data[6];
				float skullWorld7 = skullWorldMat.data[7];

				float skullWorld8 = skullWorldMat.data[8];
				float skullWorld9 = skullWorldMat.data[9];
				float skullWorld10 = skullWorldMat.data[10];
				float skullWorld11 = skullWorldMat.data[11];

				float skullWorld12 = 0;
				float skullWorld13 = 0;
				float skullWorld14 = 0;
				float skullWorld15 = 1;

				XMMATRIX skullWorldMatXM = { skullWorld0, skullWorld1, skullWorld2, skullWorld3,
											 skullWorld4, skullWorld5, skullWorld6, skullWorld7,
											 skullWorld8, skullWorld9, skullWorld10, skullWorld11,
											 skullWorld12, skullWorld13, skullWorld14, skullWorld15 };

				renderItem->world = XMMatrixTranspose(skullWorldMatXM);
				renderItem->numFramesDirty = 2;
			}
		}
		if (renderItem->numFramesDirty > 0)
		{
			ObjectCB objectCB;
			objectCB.world3x4 = renderItem->world3x4;
			objectCB.objPadding = 0;
			objectCB.world = XMMatrixTranspose(renderItem->world);
			XMMATRIX view = DirectX::XMMatrixLookAtLH(m_Eye, m_Focus, m_Up);

			XMMATRIX invViewWorld = XMMatrixTranspose(XMMatrixInverse(NULL, (view * renderItem->world)));
			objectCB.invWorld = invViewWorld;

			currentObjectCB->CopyData(renderItem->objCBIndex, objectCB);

			renderItem->numFramesDirty--;
		}
	}
}

void Graphics::UpdateMaterialCBs()
{
	auto currentMaterialCB = m_CurrFrameResource->materialCB.get();

	for (auto& material : m_Materials)
	{

		Material* mat = material.second.get();

		if (mat->numFramesDirty > 0)
		{

			MaterialCB materialCB;
			materialCB.ambient = mat->ambient;
			materialCB.shininess = mat->shininess;
			materialCB.diffuse = mat->diffuse;
			materialCB.ior = mat->ior;
			materialCB.specular = mat->specular;
			materialCB.dissolve = mat->dissolve;
			materialCB.transmittance = mat->transmittance;
			materialCB.roughness = mat->roughness;
			materialCB.emission = mat->emission;
			materialCB.metallic = mat->metallic;
			materialCB.resolution = XMFLOAT4(mat->textureResolution, 0.0f, 0.0f, 0.0f);
			materialCB.sheen = mat->sheen;
			materialCB.useTex = 0;
			materialCB.materialTransform = XMMatrixTranspose(mat->materialTransform);

			currentMaterialCB->CopyData(mat->materialCBIndex, materialCB);

			mat->numFramesDirty--;
		}
	}
}

void Graphics::UpdateGBufferPassSceneCB()
{
	XMMATRIX gBufferView, proj, invGBufView, invProj;
	float aspect;

	aspect = (float)m_D3DParams.width / (float)m_D3DParams.height;

	gBufferView = XMMatrixLookAtLH(m_Eye, m_Focus, m_Up);
	proj = XMMatrixPerspectiveFovLH(m_FOV, (float)m_D3DParams.width / (float)m_D3DParams.height, 0.1f, 100.0f);

	invGBufView = XMMatrixInverse(NULL, gBufferView);
	invProj = XMMatrixInverse(NULL, proj);

	GBufferPassSceneCB gBufferCB;
	gBufferCB.gBufferView = XMMatrixTranspose(gBufferView);
	gBufferCB.proj = XMMatrixTranspose(proj);
	gBufferCB.invGBufView = XMMatrixTranspose(invGBufView);
	gBufferCB.invProj = XMMatrixTranspose(invProj);

	auto currentGBufferPassCB = m_CurrFrameResource->gBufferPassSceneCB.get();
	currentGBufferPassCB->CopyData(0, gBufferCB);
}

void Graphics::UpdateRayTracingPassSceneCB()
{
	XMMATRIX view;

	view = DirectX::XMMatrixLookAtLH(m_Eye, m_Focus, m_Up);
	view = DirectX::XMMatrixInverse(NULL, view);

	XMFLOAT3 floatEye;
	XMStoreFloat3(&floatEye, m_Eye);

	RayTracingPassSceneCB rtPassCB;

	XMStoreFloat4x4(&rtPassCB.view, DirectX::XMMatrixTranspose(view));
	rtPassCB.viewOriginAndTanHalfFovY = XMFLOAT4(floatEye.x, floatEye.y, floatEye.z, tanf(m_FOV * 0.5f));
	rtPassCB.resolution = XMFLOAT2((float)m_D3DParams.width, (float)m_D3DParams.height);
	XMFLOAT3 floatFocus;
	XMStoreFloat3(&floatFocus, m_Focus);

	auto currentRTPassCB = m_CurrFrameResource->rayTracingPassSceneCB.get();
	currentRTPassCB->CopyData(0, rtPassCB);
}

void Graphics::UpdateLightsSceneCB()
{
	LightsSceneCB lightsCB;
	lightsCB.dirLight.direction = XMFLOAT3{ 0.f, 15.f, -15.f };
	lightsCB.dirLight.padding = 0.1f;
	lightsCB.dirLight.colour = XMFLOAT3{ 0.1f, 0.1f, 0.1f };
	lightsCB.dirLight.padding1 = 0.1f;

	auto currentLightsSceneCB = m_CurrFrameResource->lightsSceneCB.get();
	currentLightsSceneCB->CopyData(0, lightsCB);
}


void Graphics::CreateBottomLevelAS(RenderItem* renderItem1, RenderItem* renderItem2, UINT blasIndex)
{
	UINT64 objectCBAddress = m_FrameResources[m_CurrFrameResourceIndex]->objectCB.get()->Resource()->GetGPUVirtualAddress();
	UINT64 objectCBByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(ObjectCB));

	UINT64 vertexBufferStartAddress = m_Geometries["Geometry"].get()->vertexBufferGPU->GetGPUVirtualAddress();
	UINT64 indexBufferStartAddress = m_Geometries["Geometry"].get()->indexBufferGPU->GetGPUVirtualAddress();
	UINT64 vertexBufferStrideInBytes = m_Geometries["Geometry"].get()->VertexBufferView().StrideInBytes;


	const size_t rtPassRenderItemsCount = m_RayTracingPassRenderItems.size();
	
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.VertexBuffer.StartAddress = vertexBufferStartAddress + m_VertexBufferOffset;
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = vertexBufferStrideInBytes;
	geometryDesc.Triangles.VertexCount = static_cast<uint32_t>(renderItem1->vertexCount + renderItem2->vertexCount);
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometryDesc.Triangles.IndexBuffer = indexBufferStartAddress + m_IndexBufferOffset;
	geometryDesc.Triangles.IndexCount = static_cast<uint32_t>(renderItem1->indexCount + renderItem2->indexCount);
	geometryDesc.Triangles.IndexFormat = m_Geometries["Geometry"].get()->indexBufferFormat;
	geometryDesc.Triangles.Transform3x4 = 0.0f;
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

	m_VertexBufferOffset += sizeof(Vertex) * renderItem1->vertexCount;
	m_IndexBufferOffset += sizeof(uint32_t) * renderItem1->indexCount;
	m_VertexBufferOffset += sizeof(Vertex) * renderItem2->vertexCount;
	m_IndexBufferOffset += sizeof(uint32_t) * renderItem2->indexCount;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

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

	D3D12BufferInfo bufferInfo = {};
	bufferInfo.size = ASPreBuildInfo.ScratchDataSizeInBytes;
	bufferInfo.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	bufferInfo.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CreateBuffer(bufferInfo, &m_DXRObjects.BLAS[blasIndex].pScratch);
	//	CreateDefaultBuffer(nullptr, m_DXRObjects.BLAS.pScratch, bufferInfo);
#if NAME_D3D_RESOURCES
	m_DXRObjects.BLAS.pScratch->SetName(L"DXR BLAS Scratch");
#endif

	bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CreateBuffer(bufferInfo, &m_DXRObjects.BLAS[blasIndex].pResult);
#if NAME_D3D_RESOURCES
	m_DXRObjects.BLAS.pResult->SetName(L"DXR BLAS");
#endif

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = ASInputs;
	buildDesc.ScratchAccelerationStructureData = m_DXRObjects.BLAS[blasIndex].pScratch->GetGPUVirtualAddress();
	buildDesc.DestAccelerationStructureData = m_DXRObjects.BLAS[blasIndex].pResult->GetGPUVirtualAddress();

	m_D3DObjects.commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

	D3D12_RESOURCE_BARRIER uavBarrier;
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = m_DXRObjects.BLAS[blasIndex].pResult;
	uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

	m_D3DObjects.commandList->ResourceBarrier(1, &uavBarrier);
};

void Graphics::CreateTopLevelAS()
{
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc0 = {};
	instanceDesc0.InstanceContributionToHitGroupIndex = 0;
	instanceDesc0.InstanceMask = 1;
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc0.Transform), m_RayTracingPassRenderItems[0]->world);
//	instanceDesc0.Transform[0][0] = instanceDesc0.Transform[1][1] = instanceDesc0.Transform[2][2] = 1;
	instanceDesc0.AccelerationStructure = m_DXRObjects.BLAS[0].pResult->GetGPUVirtualAddress();
	instanceDesc0.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
/*
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc1 = {};
	instanceDesc1.InstanceContributionToHitGroupIndex = 1;
	instanceDesc1.InstanceMask = 1;
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc1.Transform), m_RayTracingPassRenderItems[1]->world);
//	instanceDesc1.Transform[0][0] = instanceDesc1.Transform[1][1] = instanceDesc1.Transform[2][2] = 1;
	instanceDesc1.AccelerationStructure = m_DXRObjects.BLAS[1].pResult->GetGPUVirtualAddress();
	instanceDesc1.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
*/

	D3D12_RAYTRACING_INSTANCE_DESC* descs[1] = { &instanceDesc0 };

	UINT64 instanceDescSize = ALIGN(D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT, sizeof(descs));

	D3D12BufferInfo instanceBufferInfo = {};
	instanceBufferInfo.size = instanceDescSize;
	instanceBufferInfo.heapType = D3D12_HEAP_TYPE_UPLOAD;
	instanceBufferInfo.flags = D3D12_RESOURCE_FLAG_NONE;
	instanceBufferInfo.state = D3D12_RESOURCE_STATE_GENERIC_READ;
	CreateBuffer(instanceBufferInfo, &m_DXRObjects.TLAS.pInstanceDesc);
#if NAME_D3D_RESOURCES
	m_DXRObjects.TLAS.pInstanceDesc->SetName(L"DXR TLAS Instance Descriptors");
#endif

	UINT8* pData;
	m_DXRObjects.TLAS.pInstanceDesc->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	memcpy(pData, &descs, sizeof(descs));
	m_DXRObjects.TLAS.pInstanceDesc->Unmap(0, nullptr);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
	ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	ASInputs.InstanceDescs = m_DXRObjects.TLAS.pInstanceDesc->GetGPUVirtualAddress();
	ASInputs.NumDescs = 2;
	ASInputs.Flags = buildFlags;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};

	m_D3DObjects.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

	ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
	ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

	m_DXRObjects.tlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

	D3D12BufferInfo bufferInfo = {};
	bufferInfo.size = ASPreBuildInfo.ScratchDataSizeInBytes;
	bufferInfo.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	bufferInfo.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CreateBuffer(bufferInfo, &m_DXRObjects.TLAS.pScratch);
#if NAME_D3D_RESOURCES
	m_DXRObjects.TLAS.pScratch->SetName(L"DXR TLAS Scratch");
#endif

	bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CreateBuffer(bufferInfo, &m_DXRObjects.TLAS.pResult);
#if NAME_D3D_RESOURCES
	m_DXRObjects.TLAS.pResult->SetName(L"DXR TLAS RESULT");
#endif

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = ASInputs;
	buildDesc.Inputs.InstanceDescs = m_DXRObjects.TLAS.pInstanceDesc->GetGPUVirtualAddress();
	buildDesc.ScratchAccelerationStructureData = m_DXRObjects.TLAS.pScratch->GetGPUVirtualAddress();
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
	Validate(hr, L"Failed to create DXR Output Buffer");

#if NAME_D3D_RESOURCES
	m_D3DResources.DXROutputBuffer->SetName(L"DXR Output Buffer");
#endif
}

void Graphics::CreateDescriptorHeaps()
{
	UINT objectCount = (UINT)m_AllRenderItems.size();
	UINT materialCount = (UINT)m_Materials.size();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 4u + 4u + (objectCount * gNumFrameResources) + (3u * gNumFrameResources) + (materialCount * gNumFrameResources);
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	HRESULT hr = m_D3DObjects.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_D3DResources.descriptorHeap));
	Validate(hr, L"Failed to create descriptor heap!");

	auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_D3DResources.descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	UINT handleIncrement = m_D3DObjects.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

#if NAME_D3D_RESOURCES
	m_D3DResources.descriptorHeap->SetName(L"DXR Descriptor Heap");
#endif
	
	UINT8 objectCBByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(ObjectCB));
	UINT8 materialCBByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(MaterialCB));
	UINT8 gBufferPassCBByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(GBufferPassSceneCB));
	UINT8 rayTracingPassCBByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(RayTracingPassSceneCB));
	UINT8 lightsCBByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(LightsSceneCB));

	int heapIndex = 0;
	UINT8 addressMultiplier = 0;
	for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++)
	{
		auto objectCB = m_FrameResources[m_CurrFrameResourceIndex]->objectCB->Resource();
		for (int i = 0; i < objectCount; i++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress();
			// UINT8 addressMultiplier = (objectCount * frameIndex) + i;
		//	objCBAddress = addressMultiplier * (UINT8)objectCBByteSize;


			D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {};
			constantBufferViewDesc.BufferLocation = objCBAddress;
			constantBufferViewDesc.SizeInBytes = objectCBByteSize;

			m_D3DObjects.device->CreateConstantBufferView(&constantBufferViewDesc, handle);

			handle.ptr += handleIncrement;
			objCBAddress += addressMultiplier * (UINT8)objectCBByteSize;
			addressMultiplier++;
		}
	}

	addressMultiplier = 0;

	for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++)
	{
		auto materialCB = m_FrameResources[frameIndex]->materialCB->Resource();
		for (UINT8 i = 0; i < materialCount; i++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = materialCB->GetGPUVirtualAddress();
//			UINT8 addressMultiplier = ((UINT8)materialCount * (UINT8)frameIndex) + i;
//			matCBAddress += addressMultiplier * (UINT8)materialCBByteSize;

			D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {};
			constantBufferViewDesc.BufferLocation = matCBAddress;
			constantBufferViewDesc.SizeInBytes = materialCBByteSize;

			m_D3DObjects.device->CreateConstantBufferView(&constantBufferViewDesc, handle);

			handle.ptr += handleIncrement;
			matCBAddress += addressMultiplier * (UINT8)materialCBByteSize;
			addressMultiplier++;

		}
	}
	addressMultiplier = 0;

	for (UINT8 frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++)
	{
		auto gBufferPassCB = m_FrameResources[frameIndex]->gBufferPassSceneCB->Resource();

		D3D12_GPU_VIRTUAL_ADDRESS gBufferPassCBAddress = gBufferPassCB->GetGPUVirtualAddress();
//		gBufferPassCBAddress += frameIndex * (UINT8)gBufferPassCBByteSize;

		D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {};
		constantBufferViewDesc.BufferLocation = gBufferPassCBAddress;
		constantBufferViewDesc.SizeInBytes = gBufferPassCBByteSize;

		m_D3DObjects.device->CreateConstantBufferView(&constantBufferViewDesc, handle);
		
		handle.ptr += handleIncrement;
		gBufferPassCBAddress += addressMultiplier * (UINT8)gBufferPassCBByteSize;
		addressMultiplier++;

	}
	addressMultiplier = 0;

	for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++)
	{
		auto rayTracingPassCB = m_FrameResources[frameIndex]->rayTracingPassSceneCB->Resource();
		heapIndex = heapIndex + frameIndex;
		D3D12_GPU_VIRTUAL_ADDRESS rayTracingPassCBAddress = rayTracingPassCB->GetGPUVirtualAddress();
//		rayTracingPassCBAddress += frameIndex * (UINT8)rayTracingPassCBByteSize;


		D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {};
		constantBufferViewDesc.BufferLocation = rayTracingPassCBAddress;
		constantBufferViewDesc.SizeInBytes = rayTracingPassCBByteSize;

		m_D3DObjects.device->CreateConstantBufferView(&constantBufferViewDesc, handle);

		handle.ptr += handleIncrement;
		rayTracingPassCBAddress += addressMultiplier * (UINT8)rayTracingPassCBByteSize;
		addressMultiplier++;
	}

	addressMultiplier = 0;

	for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++)
	{
		auto lightsCB = m_FrameResources[frameIndex]->lightsSceneCB->Resource();
		heapIndex = heapIndex + frameIndex;
		D3D12_GPU_VIRTUAL_ADDRESS lightsCBAddress = lightsCB->GetGPUVirtualAddress();
		//		rayTracingPassCBAddress += frameIndex * (UINT8)rayTracingPassCBByteSize;


		D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {};
		constantBufferViewDesc.BufferLocation = lightsCBAddress;
		constantBufferViewDesc.SizeInBytes = lightsCBByteSize;

		m_D3DObjects.device->CreateConstantBufferView(&constantBufferViewDesc, handle);

		handle.ptr += handleIncrement;
		lightsCBAddress += addressMultiplier * (UINT8)lightsCBByteSize;
		addressMultiplier++;
	}
/*
	for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++)
	{
		auto rayTracingPassCB = m_FrameResources[frameIndex]->rayTracingPassSceneCB->Resource();
		heapIndex = heapIndex + frameIndex;
		D3D12_GPU_VIRTUAL_ADDRESS rayTracingPassCBAddress = rayTracingPassCB->GetGPUVirtualAddress();
		rayTracingPassCBAddress += frameIndex * (UINT8)rayTracingPassCBByteSize;


		D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {};
		constantBufferViewDesc.BufferLocation = rayTracingPassCBAddress;
		constantBufferViewDesc.SizeInBytes = rayTracingPassCBByteSize;

		m_D3DObjects.device->CreateConstantBufferView(&constantBufferViewDesc, handle);

		handle.ptr += handleIncrement;
		rayTracingPassCBAddress += addressMultiplier * (UINT8)rayTracingPassCBByteSize;
		addressMultiplier++;
	}
*/
	// Create the Index Buffer SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC indexSrvDesc = {};
	indexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	indexSrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	indexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	indexSrvDesc.Buffer.StructureByteStride = 0;
	indexSrvDesc.Buffer.FirstElement = 0;
	indexSrvDesc.Buffer.NumElements = static_cast<UINT>(m_D3DValues.indicesCount * sizeof(uint32_t) / sizeof(float));
	indexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	m_D3DObjects.device->CreateShaderResourceView(m_Geometries["Geometry"].get()->indexBufferGPU, &indexSrvDesc, handle);
	handle.ptr += handleIncrement;

	// Create the Vertex Buffer SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrvDesc = {};
	vertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	vertexSrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	vertexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	vertexSrvDesc.Buffer.StructureByteStride = 0;
	vertexSrvDesc.Buffer.FirstElement = 0;
	vertexSrvDesc.Buffer.NumElements = static_cast<UINT>(m_D3DValues.vertexCount * sizeof(Vertex) / sizeof(float));
	vertexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	m_D3DObjects.device->CreateShaderResourceView(m_Geometries["Geometry"].get()->vertexBufferGPU, &vertexSrvDesc, handle);
	handle.ptr += handleIncrement;

	// Create the DXR Top Level Acceleration Structure SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = m_DXRObjects.TLAS.pResult->GetGPUVirtualAddress();

	m_D3DObjects.device->CreateShaderResourceView(nullptr, &srvDesc, handle);

	for (int i = 0; i < 1; i++)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC gBufferWorldPosSrvDesc = {};
		gBufferWorldPosSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		gBufferWorldPosSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		gBufferWorldPosSrvDesc.Texture2D.MipLevels = 1;
		gBufferWorldPosSrvDesc.Texture2D.MostDetailedMip = 0;
		gBufferWorldPosSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		handle.ptr += handleIncrement;
		m_D3DObjects.device->CreateShaderResourceView(m_D3DResources.gBufferWorldPos, &gBufferWorldPosSrvDesc, handle);

		D3D12_SHADER_RESOURCE_VIEW_DESC gBufferNormalSrvDesc = {};
		gBufferNormalSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		gBufferNormalSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		gBufferNormalSrvDesc.Texture2D.MipLevels = 1;
		gBufferNormalSrvDesc.Texture2D.MostDetailedMip = 0;
		gBufferNormalSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		handle.ptr += handleIncrement;
		m_D3DObjects.device->CreateShaderResourceView(m_D3DResources.gBufferNormal, &gBufferNormalSrvDesc, handle);


		D3D12_SHADER_RESOURCE_VIEW_DESC gBufferDiffuseSrvDesc = {};
		gBufferDiffuseSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		gBufferDiffuseSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		gBufferDiffuseSrvDesc.Texture2D.MipLevels = 1;
		gBufferDiffuseSrvDesc.Texture2D.MostDetailedMip = 0;
		gBufferDiffuseSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		handle.ptr += handleIncrement;
		m_D3DObjects.device->CreateShaderResourceView(m_D3DResources.gBufferDiffuse, &gBufferDiffuseSrvDesc, handle);


		D3D12_SHADER_RESOURCE_VIEW_DESC gBufferSpecularSrvDesc = {};
		gBufferSpecularSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		gBufferSpecularSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		gBufferSpecularSrvDesc.Texture2D.MipLevels = 1;
		gBufferSpecularSrvDesc.Texture2D.MostDetailedMip = 0;
		gBufferSpecularSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		handle.ptr += handleIncrement;
		m_D3DObjects.device->CreateShaderResourceView(m_D3DResources.gBufferSpecular, &gBufferSpecularSrvDesc, handle);
	}



	// Create DXR Output Buffer UAV 
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	handle.ptr += handleIncrement;
	m_D3DObjects.device->CreateUnorderedAccessView(m_D3DResources.DXROutputBuffer, nullptr, &uavDesc, handle);

	/*
	// Create the material texture SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC textureSrvDesc = {};
	textureSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	textureSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureSrvDesc.Texture2D.MipLevels = 1;
	textureSrvDesc.Texture2D.MostDetailedMip = 0;
	textureSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += handleIncrement;
	m_D3DObjects.device->CreateShaderResourceView(m_D3DResources.texture, &textureSrvDesc, handle);
	*/
}

ID3D12RootSignature* Graphics::CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
{
	ID3DBlob* sig;
	ID3DBlob* error;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &error);
	Validate(hr, L"Failed to serialize root signature!");

	ID3D12RootSignature* pRootSig;
	hr = m_D3DObjects.device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
	Validate(hr, L"Failed to create root signature!");

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
	Validate(hr, L"Failed to create blob from shader file!");

	//Create the compiler include handler
	IDxcIncludeHandler* dxcIncludeHandler;
	hr = m_ShaderCompilerInfo.library->CreateIncludeHandler(&dxcIncludeHandler);
	Validate(hr, L"Failed to create include handler!");

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

	Validate(hr, L"Failed to compile shader!");

	//Verify the result
	result->GetStatus(&hr);
	if (FAILED(hr))
	{
		IDxcBlobEncoding* error;
		hr = result->GetErrorBuffer(&error);
		Validate(hr, L"Failed to get shader compiler error buffer!");

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
	Validate(hr, L"Failed to get shader blob result!");
}

void Graphics::CompileShader(RtProgram& program)
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
	ranges[0].NumDescriptors = 21;
	ranges[0].RegisterSpace = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
		  	 
	ranges[1].BaseShaderRegister = 0;
	ranges[1].NumDescriptors = 7;
	ranges[1].RegisterSpace = 0;
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[1].OffsetInDescriptorsFromTableStart = 21;
		  	 
	ranges[2].BaseShaderRegister = 0;
	ranges[2].NumDescriptors = 1;
	ranges[2].RegisterSpace = 0;
	ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[2].OffsetInDescriptorsFromTableStart = 28;


	D3D12_ROOT_PARAMETER param0 = {};
	param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	param0.DescriptorTable.NumDescriptorRanges = _countof(ranges);
	param0.DescriptorTable.pDescriptorRanges = &ranges[0];

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

void Graphics::CreateShadowHitProgram()
{
	m_DXRObjects.shadowHit = HitProgram(L"ShadowHit");
	m_DXRObjects.shadowHit.chs = RtProgram(D3D12ShaderInfo(L"shaders\\ShadowRayHit.hlsl", L"", L"lib_6_3"));
	CompileShader(m_DXRObjects.shadowHit.chs);
}

void Graphics::CreateShadowMissProgram()
{
	m_DXRObjects.shadowMiss = RtProgram(D3D12ShaderInfo(L"shaders\\ShadowRayMiss.hlsl", L"", L"lib_6_3"));
	CompileShader(m_DXRObjects.shadowMiss);
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
	const WCHAR* shaderExports[] = { L"RayGen_12", L"HitGroup", L"Miss_5", };

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
	const WCHAR* rootSigExports[] = { L"RayGen_12", L"Miss_5", L"HitGroup" };

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
	Validate(hr, L"Failed to create Raytracing Pipeline State Object!");
#if NAME_D3D_RESOURCES
	m_DXRObjects.rtpso->SetName(L"DXR Pipeline State Object");
#endif

	// Get the RTPSO properties
	hr = m_DXRObjects.rtpso->QueryInterface(IID_PPV_ARGS(&m_DXRObjects.rtpsoInfo));
	Validate(hr, L"Failed to get RTPSO info object");
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
	D3D12BufferInfo bufferInfo = {};
	bufferInfo.size = shaderTableSize;
	bufferInfo.heapType = D3D12_HEAP_TYPE_UPLOAD;
	bufferInfo.state = D3D12_RESOURCE_STATE_GENERIC_READ;
	CreateBuffer(bufferInfo, &m_DXRObjects.shaderTable);
#if NAME_D3D_RESOURCES
	m_DXRObjects.shaderTable->SetName(L"DXR Shader Table");
#endif

	// Map the buffer
	uint8_t* pData = 0;
	HRESULT hr = m_DXRObjects.shaderTable->Map(0, nullptr, (void**)&pData);
	Validate(hr, L"Failed to map shader table!");

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

	/*
	// Shader Record 3 - Closest Hit program and local root parameter data (descriptor table with constant buffer and index buffer / vertex buffer pointers)
	pData += m_DXRObjects.shaderTableRecordSize;
	memcpy(pData, m_DXRObjects.rtpsoInfo->GetShaderIdentifier(L"ShadowHitGroup"), shaderIdSize);
	*/

	// Set the root parameter data. Point to start to descriptor heap.
	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = m_D3DResources.descriptorHeap->GetGPUDescriptorHandleForHeapStart();

	// Unmap
	m_DXRObjects.shaderTable->Unmap(0, nullptr);
}

void Graphics::WaitForGPU()
{
	// Schedule a signal command in the queue
	HRESULT hr = m_D3DObjects.commandQueue->Signal(m_D3DObjects.fence, m_D3DValues.fenceValues[m_D3DValues.frameIndex]);
	Validate(hr, L"Failed to signal fence!");

	// Wait until the fence has been processed
	hr = m_D3DObjects.fence->SetEventOnCompletion(m_D3DValues.fenceValues[m_D3DValues.frameIndex], m_D3DObjects.fenceEvent);
	Validate(hr, L"Failed to set fence event!");

	WaitForSingleObjectEx(m_D3DObjects.fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame
	m_D3DValues.fenceValues[m_D3DValues.frameIndex]++;
}

void Graphics::BuildGBufferCommandList()
{
	WaitForGPU();

	auto commandListAlloc = m_CurrFrameResource->CommandListAllocator;

	HRESULT hr = commandListAlloc->Reset();
	Validate(hr, L"Failed to reset G Buffer command list allocator!");

	m_D3DObjects.gBufferPassCommandList->Close();

	hr = m_D3DObjects.gBufferPassCommandList->Reset(commandListAlloc, m_D3DObjects.gBufferPassPipelineState);

	m_D3DObjects.gBufferPassCommandList->SetGraphicsRootSignature(m_D3DObjects.gBufferPassRootSignature);

	UINT rtvDescSize = m_D3DObjects.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_D3DResources.gBufferPassRTVHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_D3DResources.dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	m_D3DObjects.gBufferPassCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0.f, 0, nullptr);

	m_D3DObjects.viewport.Height = (float)m_D3DParams.height;
	m_D3DObjects.viewport.Width = (float)m_D3DParams.width;
	m_D3DObjects.viewport.TopLeftX = 0.f;
	m_D3DObjects.viewport.TopLeftY = 0.f;
	m_D3DObjects.viewport.MaxDepth = 1.f;
	m_D3DObjects.viewport.MinDepth = 0.f;
	m_D3DObjects.gBufferPassCommandList->RSSetViewports(1, &m_D3DObjects.viewport);

	m_D3DObjects.scissorRect.top = 0;
	m_D3DObjects.scissorRect.left = 0;
	m_D3DObjects.scissorRect.right = m_D3DParams.width;
	m_D3DObjects.scissorRect.bottom = m_D3DParams.height;

	m_D3DObjects.gBufferPassCommandList->RSSetScissorRects(1, &m_D3DObjects.scissorRect);

	D3D12_RESOURCE_BARRIER pBarriers[4] = {};

	pBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferWorldPos, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferNormal, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferDiffuse, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pBarriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferSpecular, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	m_D3DObjects.gBufferPassCommandList->ResourceBarrier(4, pBarriers);

//	rtvHandle.ptr = rtvHandle.ptr + (rtvDescSize * 4);

	m_D3DObjects.gBufferPassCommandList->OMSetRenderTargets(4, &rtvHandle, TRUE, &dsvHandle);

	const float clearColour[] = { 0.22f, 0.33f, 0.44f, 0.f };

	for (int i = 0; i < 4; i++)
	{
		m_D3DObjects.gBufferPassCommandList->ClearRenderTargetView(rtvHandle, clearColour, 1, &m_D3DObjects.scissorRect);

		if (i < 4) rtvHandle.ptr += rtvDescSize;
	}

	DrawRenderItems(m_GBufferPassRenderItems);

	pBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferWorldPos, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	pBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferNormal, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	pBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferDiffuse, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	pBarriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferSpecular, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	m_D3DObjects.gBufferPassCommandList->ResourceBarrier(4, pBarriers);

	SubmitGBufferCommandList();
	WaitForGPU();
}

void Graphics::SubmitGBufferCommandList()
{
	m_D3DObjects.gBufferPassCommandList->Close();

	ID3D12CommandList* pGraphicsList = { m_D3DObjects.gBufferPassCommandList };
	m_D3DObjects.commandQueue->ExecuteCommandLists(1, &pGraphicsList);
	m_D3DValues.fenceValues[m_D3DValues.frameIndex]++;
	m_CurrFrameResource->Fence = (UINT)++m_D3DValues.fenceValues[m_D3DValues.frameIndex];
	m_D3DObjects.commandQueue->Signal(m_D3DObjects.fence, m_D3DValues.fenceValues[m_D3DValues.frameIndex]);
}

void Graphics::BuildCommandList()
{
	
	D3D12_RESOURCE_BARRIER pBarriers[4] = {};
	pBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferWorldPos, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	pBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferNormal, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	pBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferDiffuse, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	pBarriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferSpecular, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	m_D3DObjects.commandList->ResourceBarrier(4, pBarriers);

	/*
	// Copy the G Buffer Pass MRT to RT Pass SRV
	m_D3DObjects.commandList->CopyResource(m_D3DResources.gBufSRVWorldPos[m_D3DValues.frameIndex], m_D3DResources.gBufferWorldPos[m_D3DValues.frameIndex]);
	m_D3DObjects.commandList->CopyResource(m_D3DResources.gBufSRVNormal[m_D3DValues.frameIndex], m_D3DResources.gBufferNormal[m_D3DValues.frameIndex]);
	m_D3DObjects.commandList->CopyResource(m_D3DResources.gBufSRVDiffuse[m_D3DValues.frameIndex], m_D3DResources.gBufferDiffuse[m_D3DValues.frameIndex]);
	m_D3DObjects.commandList->CopyResource(m_D3DResources.gBufSRVSpecular[m_D3DValues.frameIndex], m_D3DResources.gBufferSpecular[m_D3DValues.frameIndex]);
	*/


	D3D12_RESOURCE_BARRIER OutputBarriers[2] = {};
	D3D12_RESOURCE_BARRIER CounterBarriers[2] = {};
	D3D12_RESOURCE_BARRIER UAVBarriers[3] = {};

	// Transition the back buffer to a copy destination
	OutputBarriers[0].Transition.pResource = m_D3DObjects.backBuffer[m_D3DValues.frameIndex];
	OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	OutputBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// Transition the DXR Output Buffer to an unordered access state
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

	desc.HitGroupTable.StartAddress = m_DXRObjects.shaderTable->GetGPUVirtualAddress() + ((UINT)2 * m_DXRObjects.shaderTableRecordSize);
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
	
	pBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferWorldPos, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT);
	pBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferNormal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT);
	pBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferDiffuse, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT);
	pBarriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(m_D3DResources.gBufferSpecular, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT);

	m_D3DObjects.commandList->ResourceBarrier(4, pBarriers);
	

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
	m_CurrFrameResource->Fence = (UINT)++m_D3DValues.fenceValues[m_D3DValues.frameIndex];
	m_D3DObjects.commandQueue->Signal(m_D3DObjects.fence, m_D3DValues.fenceValues[m_D3DValues.frameIndex]);
}

void Graphics::Present()
{
	HRESULT hr = m_D3DObjects.swapChain->Present(m_D3DParams.vsync, 0);
	if (FAILED(hr))
	{
		hr = m_D3DObjects.device->GetDeviceRemovedReason();
		Validate(hr, L"Failed to present!");
	}
}


// Prepare to render the next frame
void Graphics::MoveToNextFrame()
{
	// Schedule a Signal command in the queue
	const UINT64 currentFenceValue = m_D3DValues.fenceValues[m_D3DValues.frameIndex];
	HRESULT hr = m_D3DObjects.commandQueue->Signal(m_D3DObjects.fence, currentFenceValue);
	Validate(hr, L"Failed to signal the command queue!");

	// Update the frame index
	m_D3DValues.frameIndex = m_D3DObjects.swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is
	if (m_D3DObjects.fence->GetCompletedValue() < m_D3DValues.fenceValues[m_D3DValues.frameIndex])
	{
		hr = m_D3DObjects.fence->SetEventOnCompletion(m_D3DValues.fenceValues[m_D3DValues.frameIndex], m_D3DObjects.fenceEvent);
		Validate(hr, L"Failed to set fence value!");

		WaitForSingleObjectEx(m_D3DObjects.fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame
	m_D3DValues.fenceValues[m_D3DValues.frameIndex] = currentFenceValue + 1;
}

void Graphics::DestroyDXRObjects()
{
	SAFE_RELEASE(m_DXRObjects.TLAS.pScratch);
	SAFE_RELEASE(m_DXRObjects.TLAS.pResult);
	SAFE_RELEASE(m_DXRObjects.TLAS.pInstanceDesc);
	SAFE_RELEASE(m_DXRObjects.BLAS[0].pScratch);
	SAFE_RELEASE(m_DXRObjects.BLAS[0].pResult);
	SAFE_RELEASE(m_DXRObjects.BLAS[0].pInstanceDesc);
	SAFE_RELEASE(m_DXRObjects.BLAS[1].pScratch);
	SAFE_RELEASE(m_DXRObjects.BLAS[1].pResult);
	SAFE_RELEASE(m_DXRObjects.BLAS[1].pInstanceDesc);
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
	SAFE_RELEASE(m_D3DResources.DXROutputBuffer);
	SAFE_RELEASE(m_Geometries["Geometry"]->vertexBufferGPU);
	SAFE_RELEASE(m_Geometries["Geometry"]->vertexBufferCPU);
	SAFE_RELEASE(m_Geometries["Geometry"]->indexBufferGPU);
	SAFE_RELEASE(m_Geometries["Geometry"]->indexBufferCPU);
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

void Graphics::RotateLeft(float amount)
{
	XMMATRIX rotationMatrix = XMMatrixRotationAxis(m_Up, XMConvertToRadians(amount));
	m_Eye = XMVector3TransformCoord(m_Eye, rotationMatrix);
}

void Graphics::RotateUp(float amount)
{
	XMMATRIX rotationMatrix = XMMatrixRotationAxis(XMVector3Normalize(XMVector3Cross(m_Eye, m_Up)), XMConvertToRadians(amount));
	m_Up = XMVector3TransformCoord(m_Up, rotationMatrix);
	m_Eye = XMVector3TransformCoord(m_Eye, rotationMatrix);
	XMVector3Normalize(m_Up);
	XMVector3Normalize(m_Eye);
}

void Graphics::ResetView()
{
	m_Eye = m_EyeInit;
	m_Focus = m_FocusInit;
	m_Up = m_UpInit;
}

void Graphics::SeedRandomVector(XMFLOAT3 randomVector)
{
	srand((UINT)time(NULL));
	float x = float(rand() % 100) + static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	srand((UINT)time(NULL));
	float y = float(rand() % 100) + static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	srand((UINT)time(NULL));
	float z = float(rand() % 100) + static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

	randomVector = XMFLOAT3{ x, y, z };
}

void Graphics::createCubeBody()
{
	
	}

void Graphics::generateContacts()
{
	
	// Create the ground plane data
	CollisionPlane plane;
	plane.direction = Vector3(0, 1, 0);
	plane.offset = 0;

	// Set up the collision data structure
	cData.Reset(maxContacts);
	cData.friction = 0.9;
	cData.restitution = 0;
	cData.tolerance = 0;

//	Matrix4 transform, otherTransform;
//	Vector3 position, otherPosition;

	if (!cData.HasMoreContacts()) return;
	if (CollisionDetector::BoxAndHalfSpace(cubeBody, plane, &cData))
	{
		cubeBody.body->SetAcceleration(Vector3(0, 0, 0));
		cubeBody.body->SetVelocity(Vector3(0, 0, 0));
	};
}

void Graphics::updateObjects(double duration)
{
	cubeBody.body->Integrate(duration);
	cubeBody.CalculateInternals();
}

void Graphics::UpdateSkinnedCBs(const GameTimer& gt)
{
	auto currSkinnedCB = m_CurrFrameResource->skinnedCB.get();

	// We only have one skinned model being animated.
	m_SkinnedModelInst->UpdateSkinnedAnimation(gt.DeltaTime());

	SkinnedCB skinnedConstants;
	copy(
		begin(m_SkinnedModelInst->FinalTransforms),
		end(m_SkinnedModelInst->FinalTransforms),
		&skinnedConstants.BoneTransforms[0]);

	currSkinnedCB->CopyData(0, skinnedConstants);
}

void Graphics::LoadSkinnedModel()
{
	vector<M3DLoader::SkinnedVertex> vertices;
	vector <uint16_t> indices;

	M3DLoader m3dLoader;
	m3dLoader.LoadM3d("models/soldier.m3d", vertices, indices,
		m_SkinnedSubsets, m_SkinnedMats, m_SkinnedInfo);

	m_SkinnedModelInst = make_unique<SkinnedModelInstance>();
	m_SkinnedModelInst->SkinnedInfo = &m_SkinnedInfo;
	m_SkinnedModelInst->FinalTransforms.resize(m_SkinnedInfo.BoneCount());
	m_SkinnedModelInst->ClipName = "Take1";
	m_SkinnedModelInst->TimePos = 0.0f;

	auto memory = malloc(sizeof(unique_ptr<Model>));

	auto model = new (memory) unique_ptr<Model>();
	*model = std::make_unique<Model>();
	auto material = std::make_unique<Material>();

	auto geo = make_unique<MeshGeometry>();
	geo->name = m_SkinnedModelFilename;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);
	
	geo->vertexByteStride = sizeof(Vertex);
	geo->vertexBufferByteSize = vbByteSize;
	geo->indexBufferFormat = DXGI_FORMAT_R16_UINT;
	geo->indexBufferByteSize = ibByteSize;

	for (UINT i = 0; i < (UINT)m_SkinnedSubsets.size(); ++i)
	{
		unique_ptr<SubmeshGeometry> submesh;
		string name = "sm_" + to_string(i);

		submesh->indexCount = (UINT)m_SkinnedSubsets[i].FaceCount * 3;
		submesh->startIndexLocation = m_SkinnedSubsets[i].FaceStart * 3;
		submesh->baseVertexLocation = 0;

		m_Geometries[geo->name]->drawArgs[m_SkinnedModelFilename] = move(submesh);
	}

	m_D3DValues.vertexCount += (UINT)model->get()->vertices.size();
	m_D3DValues.indicesCount += (UINT)model->get()->indices.size();


	m_Geometries[geo->name] = move(geo);

/**
 * TODO Check this is done by build mesh geometry

	//TODO may need to fix vertex and replace with skinned vertex
	
	Validate(D3DCreateBlob(vbByteSize, &geo->vertexBufferCPU), L"Failed to create Skinned Model Vertex Buffer CPU");
	CopyMemory(geo->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	Validate(D3DCreateBlob(ibByteSize, &geo->indexBufferCPU), L"Failed to create Skinned Model Index Buffer CPU");
	CopyMemory(geo->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
	
	geo->vertexBufferGPU = CreateDefaultBuffer(vertices.data(),

	// Use below

	auto memory = malloc(sizeof(unique_ptr<Model>));

	auto model = new (memory) unique_ptr<Model>();
	*model = std::make_unique<Model>();
	auto material = std::make_unique<Material>();

	Validate(D3DCreateBlob(vb))
	
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

			vertex.normal =
			{
				attrib.normals[3 * index.normal_index + 2],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 0]
			};

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(model->get()->vertices.size());
				model->get()->vertices.push_back(vertex);
			}

			model->get()->indices.push_back(uniqueVertices[vertex]);
		};
	}

	unique_ptr<SubmeshGeometry> submeshGeo = make_unique<SubmeshGeometry>();

	submeshGeo->indexCount = model->get()->indices.size();
	submeshGeo->startIndexLocation = m_D3DValues.indicesCount;
	submeshGeo->vertexCount = (UINT)model->get()->vertices.size();
	submeshGeo->baseVertexLocation = m_D3DValues.vertexCount;


	m_Models[filename] = std::move(*model);
	m_Materials[filename] = std::move(material);

	m_Geometries[geometryName]->drawArgs[filename] = std::move(submeshGeo);
 */

}
