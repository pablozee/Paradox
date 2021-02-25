#pragma once
#include <Windows.h>
#include <DirectXMath.h>
#include "dxc/dxcapi.h"
#include "dxc/dxcapi.use.h"
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>
#include <string>

#include "../Structures.h"

using namespace DirectX;

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

struct SceneCB
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
	float sheen;
	int useTex;
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
	ID3D12GraphicsCommandList4* commandList = nullptr;
	ID3D12CommandAllocator* rasterCommandAllocators[2] = { nullptr, nullptr };
	ID3D12GraphicsCommandList* rasterCommandList;

	ID3D12Fence* fence = nullptr;
	HANDLE fenceEvent;

	IDXGISwapChain3* swapChain = nullptr;
	ID3D12Resource* backBuffer[2] = { nullptr, nullptr };

	ID3D12PipelineState* pipelineState = nullptr;

	ID3D12RootSignature* rootSignature = nullptr;

	ID3D12Resource* depthStencilView = nullptr;

	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
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
	ID3D12DescriptorHeap*		dsvDescriptorHeap = nullptr;

	ID3D12Resource*				depthStencilView = nullptr;

	ID3D12Resource*				vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	vertexBufferView;

	ID3D12Resource*				indexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW		indexBufferView;

	ID3D12Resource*				texture = nullptr;
	ID3D12Resource*				textureUploadResource = nullptr;

	ID3D12Resource*				sceneCB;
	SceneCB						sceneCBData[2];
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

	ID3D12Resource*				gBufferWorldPos = nullptr;
	ID3D12Resource*				gBufferNormal = nullptr;
	ID3D12Resource*				gBufferDiffuse = nullptr;
	ID3D12Resource*				gBufferSpecular = nullptr;
	ID3D12Resource*				gBufferReflectivity = nullptr;
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

