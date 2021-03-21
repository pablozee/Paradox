#pragma once
#include "core.h"
#include "../Helpers.h"
#include "D3D12Structures.h"

template<typename T>
class UploadBuffer
{
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer)
		:
		isConstantBuffer(isConstantBuffer)
	{
		elementByteSize = sizeof(T);

		if (isConstantBuffer) elementByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(m_D3DResources.sceneCBData));

		HRESULT hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(elementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadBuffer)));

		Helpers::Validate(hr, "Failed to create buffer!");

		hr = uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>&mappedData);

		Helpers::Validate(hr, "Failed to map buffer!");

		// We do not need to unmap until we are done with the resource but must not write to the resource while it is being used by the GPU
		// so we must efficiently synchronize
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

	~UploadBuffer()
	{
		if (uploadBuffer != nullptr) uploadBuffer->Unmap(0, nullptr);

		mappedData = nullptr;
	}

	ID3D12Resource* Resource() const
	{
		return uploadBuffer;
	}

	void CopyData(int elementIndex, const T& data)
	{
		memcpy(&mappedData[elementIndex * elementByteSize], &data, sizeof(T));
	}

private:
	ID3D12Resource* uploadBuffer;
	BYTE* mappedData = nullptr;

	UINT elementByteSize = 0;
	bool isConstantBuffer = false;
};