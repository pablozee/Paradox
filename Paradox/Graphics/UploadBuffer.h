#pragma once
#include "core.h"

#define ALIGN(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)

template<typename T>
class UploadBuffer
{
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer)
		:
		isConstantBuffer(isConstantBuffer)
	{
		elementByteSize = sizeof(T);

		if (isConstantBuffer) elementByteSize = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(T));
	//	if (isConstantBuffer) elementByteSize = CalcConstantBufferByteSize(sizeof(T));

		HRESULT hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(elementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer));

		Validate(hr, L"Failed to create buffer!");

		hr = uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));

		Validate(hr, L"Failed to map buffer!");

		// We do not need to unmap until we are done with the resource but must not write to the resource while it is being used by the GPU
		// so we must efficiently synchronize
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

	UploadBuffer::~UploadBuffer()
	{
		if (uploadBuffer != nullptr) uploadBuffer->Unmap(0, nullptr);

		mappedData = nullptr;
	}

	ID3D12Resource* UploadBuffer::Resource() const
	{
		return uploadBuffer;
	}

	void CopyData(int elementIndex, const T& data)
	{
		memcpy(&mappedData[elementIndex * elementByteSize], &data, sizeof(T));
	}

	void Validate(HRESULT hr, LPWSTR message)
	{
		if (FAILED(hr))
		{
			MessageBox(NULL, message, L"Error", MB_OK);
			PostQuitMessage(EXIT_FAILURE);
		}
	}

	static UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		return (byteSize + 255) & ~255;
	}

private:
	ID3D12Resource* uploadBuffer;
	BYTE* mappedData = nullptr;

	UINT elementByteSize = 0;
	bool isConstantBuffer = false;
};

