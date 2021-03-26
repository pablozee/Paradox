#pragma once

#include "core.h"
#include "../Structures.h"

using namespace DirectX;

using namespace std;

struct SubmeshGeometry
{
	UINT indexCount = 0;
	UINT startIndexLocation = 0;
	UINT baseVertexLocation = 0;

	BoundingBox bounds;
};

struct MeshGeometry
{
	string						name;

	ID3DBlob* vertexBufferCPU;
	ID3DBlob* indexBufferCPU;

	ID3D12Resource* vertexBufferGPU = nullptr;
	ID3D12Resource* vertexBufferUploader = nullptr;

	ID3D12Resource* indexBufferGPU = nullptr;
	ID3D12Resource* indexBufferUploader = nullptr;

	UINT						vertexByteStride = 0;
	UINT						vertexBufferByteSize = 0;
	DXGI_FORMAT					indexBufferFormat = DXGI_FORMAT_R32_UINT;
	UINT						indexBufferByteSize = 0;

	// Container holding submesh for each geometry in index / vertex buffer
	std::unordered_map<std::string, SubmeshGeometry> drawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
	{
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		vertexBufferView.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = vertexByteStride;
		vertexBufferView.SizeInBytes = vertexBufferByteSize;

		return vertexBufferView;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
	{
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		indexBufferView.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
		indexBufferView.Format = indexBufferFormat;
		indexBufferView.SizeInBytes = indexBufferByteSize;

		return indexBufferView;
	}

	void DisposeUploaders()
	{
		vertexBufferUploader = nullptr;
		indexBufferUploader = nullptr;
	}

};

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	XMMATRIX world = XMMatrixIdentity();

	int numFramesDirty = gNumFrameResources;

	INT objCBIndex = -1;
	INT matCBIndex = -1;

	Model model;
	Material* material = nullptr;
	MeshGeometry* geometry = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT indexCount = 0;
	UINT startIndexLocation = 0;
	int baseVertexLocation = 0;
};

struct ObjectCB
{
	XMMATRIX world = XMMatrixIdentity();
};

struct GBufferPassSceneCB
{
	XMMATRIX gBufferView = XMMatrixIdentity();
	XMMATRIX proj = XMMatrixIdentity();
};

struct RayTracingPassSceneCB
{
	XMMATRIX view = XMMatrixIdentity();
	XMFLOAT4 viewOriginAndTanHalfFovY = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	XMFLOAT2 resolution = XMFLOAT2(1280, 960);
	float numDirLights;
	float numPointLights;
	XMFLOAT3 randomSeedVector0;
	float padding;
	XMFLOAT3 randomSeedVector1;
	float padding1;
	DirectionalLight directionalLights[10];
	PointLight pointLights[10];
};

struct MaterialCB
{
	XMFLOAT3 ambient;
	float shininess;
	XMFLOAT3 diffuse;
	float ior;
	XMFLOAT3 specular;
	float dissolve;
	XMFLOAT3 transmittance;
	float roughness;
	XMFLOAT3 emission;
	float metallic;
	XMFLOAT4 resolution;
	XMMATRIX materialTransform;
	float sheen;
	int useTex;

};

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
	UINT indicesCount = 0;
	UINT vertexCount = 0;
};

struct D3D12Objects
{
	IDXGIFactory4* factory = nullptr;
	IDXGIAdapter1* adapter = nullptr;
	ID3D12Device5* device = nullptr;

	ID3D12CommandQueue* commandQueue = nullptr;
	ID3D12CommandAllocator* commandAllocators[2] = { nullptr, nullptr };
	ID3D12GraphicsCommandList4* commandList = nullptr;
	ID3D12CommandAllocator* gBufferPassCommandAllocators[2] = { nullptr, nullptr };
	ID3D12GraphicsCommandList* gBufferPassCommandList = nullptr;

	ID3D12Fence* fence = nullptr;
	HANDLE fenceEvent;

	IDXGISwapChain3* swapChain = nullptr;
	ID3D12Resource* backBuffer[2] = { nullptr, nullptr };

	ID3D12RootSignature* rootSignature = nullptr;
	ID3D12RootSignature* gBufferPassRootSignature = nullptr;

	ID3D12PipelineState* gBufferPassPipelineState;

	ID3D12Resource* depthStencilView = nullptr;

	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
};

struct D3D12ShaderCompilerInfo
{
	dxc::DxcDllSupport DxcDllHelper;
	IDxcCompiler* compiler = nullptr;
	IDxcLibrary* library = nullptr;
};

struct D3D12ShaderInfo
{
	LPCWSTR		filename = nullptr;
	LPCWSTR		entryPoint = nullptr;
	LPCWSTR		targetProfile = nullptr;
	LPCWSTR*	arguments = nullptr;
	DxcDefine*  defines = nullptr;
	UINT32		argCount = 0;
	UINT32		defineCount = 0;

	D3D12ShaderInfo()
	{}

	D3D12ShaderInfo(LPCWSTR inFilename, LPCWSTR inEntryPoint, LPCWSTR inProfile)
	{
		filename = inFilename;
		entryPoint = inEntryPoint;
		targetProfile = inProfile;
	}
};

struct D3D12Resources
{
	ID3D12DescriptorHeap*		rtvHeap;
	ID3D12DescriptorHeap*		gBufferPassRTVHeap;
	ID3D12DescriptorHeap*		dsvDescriptorHeap = nullptr;

	ID3D12Resource*				depthStencilView = nullptr;

	ID3D12Resource*				texture = nullptr;
	ID3D12Resource*				textureUploadResource = nullptr;

	ID3D12Resource*				sceneCB;
	GBufferPassSceneCB			sceneCBData[2];
	UINT8*						sceneCBStart = nullptr;

	ID3D12Resource*				materialCB = nullptr;
	MaterialCB					materialCBData;
	UINT8*						materialCBStart = nullptr;

	ID3D12Resource*				DXROutputBuffer;

	ID3D12DescriptorHeap*		descriptorHeap;

	float						translationOffset = 0;
	float						rotationOffset = 0;
	XMFLOAT3					eyeAngle{ 0.f, 0.f, 0.f };

	ID3D12DescriptorHeap*		gBufferDescHeap = nullptr;

	ID3D12Resource*				gBufferWorldPos[2];
	ID3D12Resource*				gBufferNormal[2];
	ID3D12Resource*				gBufferDiffuse[2];
	ID3D12Resource*				gBufferSpecular[2];
	ID3D12Resource*				gBufferReflectivity[2];
};

struct AccelerationStructureBuffer
{
	ID3D12Resource* pScratch = nullptr;
	ID3D12Resource* pResult = nullptr;
	ID3D12Resource* pInstanceDesc = nullptr;
};

struct RtProgram
{
	D3D12ShaderInfo			info = {};
	IDxcBlob*				blob = nullptr;
	ID3D12RootSignature*	pRootSignature = nullptr;

	D3D12_DXIL_LIBRARY_DESC	dxilLibDesc;
	D3D12_EXPORT_DESC		exportDesc;
	D3D12_STATE_SUBOBJECT	subobject;
	std::wstring			exportName;

	RtProgram()
	{
		exportDesc.ExportToRename = nullptr;
	}

	RtProgram(D3D12ShaderInfo shaderInfo)
	{
		info = shaderInfo;
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		exportName = shaderInfo.entryPoint;
		exportDesc.ExportToRename = nullptr;
		exportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
	}

	void SetBytecode()
	{
		exportDesc.Name = exportName.c_str();

		dxilLibDesc.NumExports = 1;
		dxilLibDesc.pExports = &exportDesc;
		dxilLibDesc.DXILLibrary.BytecodeLength = blob->GetBufferSize();
		dxilLibDesc.DXILLibrary.pShaderBytecode = blob->GetBufferPointer();

		subobject.pDesc = &dxilLibDesc;
	}
};

struct HitProgram
{
	RtProgram ahs;
	RtProgram chs;

	std::wstring exportName;
	D3D12_HIT_GROUP_DESC desc = {};
	D3D12_STATE_SUBOBJECT subobject = {};

	HitProgram() {}

	HitProgram(LPCWSTR name)
		:
		exportName(name)
	{
		desc = {};
		desc.HitGroupExport = exportName.c_str();
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		subobject.pDesc = &desc;
	}

	void SetExports(bool anyHit)
	{
		desc.HitGroupExport = exportName.c_str();
		if (anyHit) desc.AnyHitShaderImport = ahs.exportDesc.Name;
		desc.ClosestHitShaderImport = chs.exportDesc.Name;
	}
};

struct DXRObjects
{
	AccelerationStructureBuffer		TLAS;
	AccelerationStructureBuffer		BLAS;
	uint64_t						tlasSize;

	ID3D12Resource*					shaderTable = nullptr;
	uint32_t						shaderTableRecordSize = 0;

	RtProgram						rgs;
	RtProgram						miss;
	HitProgram						hit;

	ID3D12StateObject*				rtpso = nullptr;
	ID3D12StateObjectProperties*	rtpsoInfo = nullptr;
};

struct D3D12BufferCreateInfo
{
	UINT64 size = 0;
	UINT64 alignment = 0;
	D3D12_HEAP_TYPE defaultBufferHeapType = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS defaultBufferHeapFlags = D3D12_HEAP_FLAG_NONE;
	D3D12_RESOURCE_FLAGS defaultBufferResourceFlags = D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES defaultBufferState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_HEAP_TYPE uploadBufferHeapType = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_HEAP_FLAGS uploadBufferFlags = D3D12_HEAP_FLAG_NONE;
	D3D12_RESOURCE_FLAGS uploadBufferResourceFlags = D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES uploadBufferState = D3D12_RESOURCE_STATE_GENERIC_READ;
	D3D12_RESOURCE_STATES defaultBufferFinalState = D3D12_RESOURCE_STATE_GENERIC_READ;

	D3D12BufferCreateInfo() {};

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_HEAP_FLAGS InDefaultFlags, D3D12_HEAP_FLAGS InUploadFlags, D3D12_RESOURCE_STATES defaultFinalState)
		:
		size(InSize),
		defaultBufferHeapFlags(InDefaultFlags),
		uploadBufferFlags(InUploadFlags),
		defaultBufferFinalState(defaultFinalState)
	{};

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_HEAP_FLAGS InDefaultFlags, D3D12_HEAP_FLAGS InUploadFlags, D3D12_RESOURCE_STATES InDefaultState, D3D12_RESOURCE_STATES InUploadState, D3D12_RESOURCE_STATES defaultFinalState)
		:
		size(InSize),
		defaultBufferHeapFlags(InDefaultFlags),
		defaultBufferState(InDefaultState),
		uploadBufferFlags(InUploadFlags),
		uploadBufferState(InUploadState),
		defaultBufferFinalState(defaultFinalState)
	{};

	D3D12BufferCreateInfo(UINT64 InSize, D3D12_HEAP_TYPE InDefaultHeapType, D3D12_RESOURCE_STATES InDefaultState, D3D12_HEAP_TYPE InUploadHeapType, D3D12_RESOURCE_STATES InUploadState, D3D12_RESOURCE_STATES defaultFinalState)
		:
		size(InSize),
		defaultBufferHeapType(InDefaultHeapType),
		defaultBufferState(InDefaultState),
		uploadBufferHeapType(InUploadHeapType),
		uploadBufferState(InUploadState),
		defaultBufferFinalState(defaultFinalState)
	{};

	D3D12BufferCreateInfo(UINT64 InSize, UINT64 InAlignment, D3D12_HEAP_TYPE InDefaultHeapType, 
						  D3D12_RESOURCE_STATES InDefaultState, D3D12_HEAP_FLAGS InDefaultFlags, 
						  D3D12_HEAP_TYPE InUploadHeapType, D3D12_RESOURCE_STATES InUploadState, 
						  D3D12_HEAP_FLAGS InUploadFlags, D3D12_RESOURCE_STATES defaultFinalState)
		:
		size(InSize),
		alignment(InAlignment),
		defaultBufferHeapType(InDefaultHeapType),
		defaultBufferState(InDefaultState),
		defaultBufferHeapFlags(InDefaultFlags),
		uploadBufferHeapType(InUploadHeapType),
		uploadBufferState(InUploadState),
		uploadBufferFlags(InUploadFlags),
		defaultBufferFinalState(defaultFinalState)
	{};
};

static const D3D12_HEAP_PROPERTIES UploadHeapProperties =
{
	D3D12_HEAP_TYPE_UPLOAD,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0,
	0

};

static const D3D12_HEAP_PROPERTIES DefaultHeapProperties =
{
	D3D12_HEAP_TYPE_DEFAULT,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0,
	0
};

