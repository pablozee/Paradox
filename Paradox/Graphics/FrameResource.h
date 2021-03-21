#pragma once
#include "D3D12Structures.h"
#include "UploadBuffer.h"

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
	float sheen;
	int useTex;
};

class FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	ID3D12CommandAllocator* CommandListAllocator;

	// Constant Buffer per frame as we cannot update the buffer until the GPU is done processing the commands that use it
	std::unique_ptr <UploadBuffer<ObjectCB>> objectCB = nullptr;
	std::unique_ptr <UploadBuffer<MaterialCB>> materialCB = nullptr;
	std::unique_ptr <UploadBuffer<GBufferPassSceneCB>> gBufferPassSceneCB = nullptr;
	std::unique_ptr <UploadBuffer<RayTracingPassSceneCB>> rayTracingPassSceneCB = nullptr;

	// Fence value allows us to check if frame resource is in use by GPU
	UINT Fence = 0;
};