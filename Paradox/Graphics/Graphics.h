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
	void Initialize_Shader_Compiler(D3D12ShaderCompilerInfo &shaderCompilerInfo);

private:
	D3D12Objects m_D3dObjects = {};
	D3D12ShaderCompilerInfo m_ShaderCompilerInfo;
};