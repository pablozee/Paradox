#pragma once
#include <Windows.h>
#include "D3D12Structures.h"
#include "../Structures.h"

class Graphics
{
public:
	Graphics(Config config);
	~Graphics();

	void Init(HWND hwnd);
	void Shutdown();

private:
	void LoadModel(std::string filepath, Model &model, Material &material);

	void InitializeShaderCompiler();
	
	void CreateDevice();
	void CreateCommandQueue();
	void CreateCommandAllocator();
	void CreateFence();
	void CreateSwapChain(HWND hwnd);
	void CreateCommandList();
	void ResetCommandList();

	void CreateDescriptorHeaps();
	void CreateBackBufferRtv();
	void CreateBuffer(D3D12BufferCreateInfo& info, ID3D12Resource** ppResource);
	void CreateVertexBuffer(Model &model);
	void CreateIndexBuffer(Model &model);
	
	void CreateTexture(Material &material);
	void UploadTexture(ID3D12Resource* destResource, ID3D12Resource* srcResource, const TextureInfo& texture);

	void CreateConstantBuffer(ID3D12Resource** buffer, UINT64 size);
	void CreateViewCB();
	void CreateMaterialConstantBuffer(const Material& material);

private:
	D3D12Params m_D3DParams;
	D3D12Objects m_D3DObjects;
	D3D12Values m_D3DValues;
	D3D12ShaderCompilerInfo m_ShaderCompilerInfo;
	D3D12Resources m_D3DResources;
	Model model;
	Material material;
};