#pragma once
#include <Windows.h>
#include "D3D12Structures.h"

class Graphics
{
public:
	Graphics();
	~Graphics();

	void Init(HWND m_Hwnd);
	void Shutdown();

private:
	void InitializeShaderCompiler(D3D12ShaderCompilerInfo &m_ShaderCompilerInfo);
	void CreateDevice(D3D12Objects m_D3DObjects);

private:
	D3D12Objects m_D3DObjects;
	D3D12ShaderCompilerInfo m_ShaderCompilerInfo;
};