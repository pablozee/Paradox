#include "Graphics.h"
#include "../Helpers.h"
#include "dxc/dxcapi.h"
#include "dxc/dxcapi.use.h"

Graphics::Graphics()
{}

Graphics::~Graphics()
{
	Shutdown();
}

void Graphics::Init(HWND m_Hwnd)
{
	Initialize_Shader_Compiler(m_ShaderCompilerInfo);
}

void Graphics::Shutdown()
{

}

void Graphics::Initialize_Shader_Compiler(D3D12ShaderCompilerInfo &shaderCompilerInfo)
{
	HRESULT hr = shaderCompilerInfo.DxcDllHelper.Initialize();
	Helpers::Validate(hr, L"Failed to initialize DxCDllSupport!");

	hr = shaderCompilerInfo.DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &shaderCompilerInfo.compiler);
	Helpers::Validate(hr, L"Failed to create DxcCompiler!");

	hr = shaderCompilerInfo.DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &shaderCompilerInfo.library);
	Helpers::Validate(hr, L"Failed to create DxcLibrary!");
}