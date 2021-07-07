#pragma once
#include "D3D12Structures.h"
#include "UploadBuffer.h"

struct FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT skinnedObjectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	ID3D12CommandAllocator* CommandListAllocator;

	// Constant Buffer per frame as we cannot update the buffer until the GPU is done processing the commands that use it
	std::unique_ptr<UploadBuffer<ObjectCB>> objectCB = nullptr;
	std::unique_ptr<UploadBuffer<SkinnedCB>> skinnedCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialCB>> materialCB = nullptr;
	std::unique_ptr<UploadBuffer<GBufferPassSceneCB>> gBufferPassSceneCB = nullptr;
	std::unique_ptr<UploadBuffer<RayTracingPassSceneCB>> rayTracingPassSceneCB = nullptr;
	std::unique_ptr<UploadBuffer<LightsSceneCB>> lightsSceneCB = nullptr;

	// Fence value allows us to check if frame resource is in use by GPU
	UINT Fence = 0;
};
