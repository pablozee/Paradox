#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT skinnedObjectCount, UINT materialCount)
{
	HRESULT hr = device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&CommandListAllocator)
	);

	objectCB = std::make_unique<UploadBuffer<ObjectCB>>(device, objectCount, true);
	skinnedCB = std::make_unique<UploadBuffer<SkinnedCB>>(device, skinnedObjectCount, true);
	materialCB = std::make_unique<UploadBuffer<MaterialCB>>(device, materialCount, true);
	gBufferPassSceneCB = std::make_unique<UploadBuffer<GBufferPassSceneCB>>(device, passCount, true);
	rayTracingPassSceneCB = std::make_unique<UploadBuffer<RayTracingPassSceneCB>>(device, passCount, true);
	lightsSceneCB = std::make_unique<UploadBuffer<LightsSceneCB>>(device, passCount, true);
}

FrameResource::~FrameResource()
{

}