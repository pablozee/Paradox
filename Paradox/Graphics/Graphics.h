#pragma once
#include "core.h"
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
	void Validate(HRESULT hr, LPWSTR message);
	void LoadModel(std::string filepath, MeshGeometry* geometry);
	TextureInfo LoadTexture(std::string filepath);
	void FormatTexture(TextureInfo& info, UINT8* pixels);

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
	
	void CreateBuffer(D3D12BufferInfo& info, ID3D12Resource** ppResource);


	void BuildMeshGeometry(std::string geometryName);
	ID3D12Resource* CreateDefaultBuffer(const void* initData, ID3D12Resource* uploadBuffer, D3D12BufferCreateInfo bufferCreateInfo);
	void BuildRenderItems();
	void DrawRenderItems(const std::vector<RenderItem*>& renderItems);

	void CreateTexture(Material &material);
	void UploadTexture(ID3D12Resource* destResource, ID3D12Resource* srcResource, const TextureInfo& texture);

	void BuildFrameResources();
	
	void UpdateObjectCBs();
	void UpdateMaterialCBs();
	void UpdateGBufferPassSceneCB();
	void UpdateRayTracingPassSceneCB();

	void SeedRandomVector(XMFLOAT3 seed);

	void CreateBottomLevelAS();
	void CreateTopLevelAS();
	void CreateDXROutput();

	void CreateDescriptorHeaps();

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

	void CreateRTRootSignature();

	void WaitForGPU();

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
	XMVECTOR m_EyeInit = { 0.f, 0.f, -17.0f };
	XMVECTOR m_FocusInit = { 0.f, 0.f, 0.f };
	XMVECTOR m_UpInit = { 0.f, 1.f, 0.f };
	XMVECTOR m_Eye = m_EyeInit;
	XMVECTOR m_Focus = m_FocusInit;
	XMVECTOR m_Up = m_UpInit;
	float m_FOV = 65.f * (XM_PI / 180.f);
	XMFLOAT3 m_RandomVectorSeed0;
	XMFLOAT3 m_RandomVectorSeed1;

	std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
	FrameResource* m_CurrFrameResource = nullptr;
	int m_CurrFrameResourceIndex = 0;

	std::vector<std::unique_ptr<RenderItem>> m_AllRenderItems;

	// Render items per PSO
	std::vector<RenderItem*> m_GBufferPassRenderItems;
	std::vector<RenderItem*> m_RayTracingPassRenderItems;

	MeshGeometry* m_Geometry;
	std::unordered_map<std::string, std::unique_ptr<Model>> m_Models;
	std::unordered_map<std::string, std::unique_ptr<Material>> m_Materials;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometries;
};