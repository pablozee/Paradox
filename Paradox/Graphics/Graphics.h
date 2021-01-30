#pragma once
#include <Windows.h>
#include "D3D12Structures.h"

class Graphics
{
public:
	Graphics();
	~Graphics();

	void Init();
	void Shutdown();

private:
	void InitializeShaderCompiler();
	void CreateDevice();
	void CreateCommandQueue();

private:
	D3D12Objects m_D3DObjects;
	D3D12ShaderCompilerInfo m_ShaderCompilerInfo;
};