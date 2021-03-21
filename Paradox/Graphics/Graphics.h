#pragma once
#include <Windows.h>
#include "../Structures.h"
#include "D3D12Structures.h"
#include "FrameResource.h"

class Graphics
{
public:
	Graphics(Config config);
	~Graphics();

	void Init(HWND hwnd);
	void Update();
	void Render();
	void Shutdown();

	void RotateLeft(float amount);
	void RotateUp(float amount);

	void ResetView();

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

	void CreateGBufferPassCommandAllocator();
	void CreateGBufferPassCommandList();
	void ResetGBufferPassCommandList();
	void CreateDSVDescriptorHeap();

	void CreateGBufferPassRootSignature();

	void CreateRTVDescriptorHeaps();
	void CreateBackBufferRTV();
	void CreateBuffer(D3D12BufferCreateInfo& info, ID3D12Resource** ppResource);
	void CreateVertexBuffer(Model &model);
	void CreateIndexBuffer(Model &model);
	
	void CreateTexture(Material &material);
	void UploadTexture(ID3D12Resource* destResource, ID3D12Resource* srcResource, const TextureInfo& texture);

	void BuildFrameResources();

//	void CreateConstantBuffer(ID3D12Resource** buffer, UINT64 size, bool perFrame);
//	void CreateSceneCB();
	void UpdateObjectCBs();
	void UpdateGBufferPassSceneCB();
	void UpdateRayTracingPassSceneCB();

	void SeedRandomVector(XMFLOAT3 seed);
//	void CreateMaterialConstantBuffer(const Material& material);

	void CreateBottomLevelAS();
	void CreateTopLevelAS();
	void CreateDXROutput();

	void CreateDescriptorHeaps(const Model &model);

	void CreateDepthStencilView();
	void CompileGBufferPassShaders();

	void CreateGBufferPassPSO();
	void CreateGBufferPassRTVDescriptorHeaps();
	void CreateGBufferPassRTVResources();
	void CreateGBufferPassRTVs();

	void CompileShader(RtProgram &program);
	void CompileShader(D3D12ShaderInfo &info, IDxcBlob** blob);
	ID3D12RootSignature* CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc);
	void CreateRayGenProgram();
	void CreateMissProgram();
	void CreateClosestHitProgram();
	
	void CreatePipelineStateObject();
	void CreateShaderTable();

	void WaitForGPU();

	void UpdateSceneCB();

	void BuildGBufferCommandList();
	void SubmitGBufferCommandList();
	void ResetGBufferCommandList();

	void BuildCommandList();
	void SubmitCommandList();

	void Present();
	void MoveToNextFrame();

	void DestroyDXRObjects();
	void DestroyD3D12Resources();
	void DestroyShaders();
	void DestroyD3D12Objects();

private:
	D3D12Params m_D3DParams;
	D3D12Objects m_D3DObjects;
	DXRObjects m_DXRObjects;
	D3D12Values m_D3DValues;
	D3D12ShaderCompilerInfo m_ShaderCompilerInfo;
	D3D12Resources m_D3DResources;
	UINT m_FrameCount = 2;
	Model m_Model;
	Material m_Material;
	XMVECTOR m_EyeInit = { 17.f, 0.f, 0.f };
	XMVECTOR m_FocusInit = { 0.f, 0.f, 0.f };
	XMVECTOR m_UpInit = { 0.f, 1.f, 0.f };
	XMVECTOR m_Eye = m_EyeInit;
	XMVECTOR m_Focus = m_FocusInit;
	XMVECTOR m_Up = m_UpInit;
	XMFLOAT3 m_RandomVectorSeed0;
	XMFLOAT3 m_RandomVectorSeed1;

	std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
	FrameResource* m_CurrFrameResource = nullptr;
	int m_CurrFrameResourceIndex = 0;

	std::vector<std::unique_ptr<RenderItem>> m_AllRenderItems;
};