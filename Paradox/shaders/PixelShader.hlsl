/**/

struct DirectionalLight
{
	float3			 directionalLightDirection;
	float			 directionalLightPadding;
	float3			 directionalLightColour;
	float			 directionalLightPadding1;
};

struct PointLight
{
	float3			 pointLightPosition;
	float			 pointLightPadding;
	float3			 pointLightColour;
	float			 pointLightPadding1;
};

cbuffer SceneCB : register(b0)
{
	matrix			 view;
	float4			 viewOriginAndTanHalfFovY;
	float2			 resolution;
	float			 numDirLights;
	float			 numPointLights;
	float3			 randomSeedVector0;
	float			 padding;
	float3			 randomSeedVector1;
	float			 padding1;
	DirectionalLight directionalLights[10];
	PointLight		 pointLights[10];
}

cbuffer MaterialCB : register(b1)
{
	float3			 ambient;
	float			 shininess;
	float3			 diffuse;
	float			 ior;
	float3			 specular;
	float			 dissolve;
	float3			 transmittance;
	float			 roughness;
	float3			 emission;
	float			 metallic;
	float4			 textureResolution;
	float			 sheen;
	int				 useTex;
}

struct PSInput
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float2 TexC : TEXCOORD;
	float3 NormalW : NORMAL;
};

struct PSOutput
{
	float4 gBufferWorldPos		 : SV_Target0;
	float4 gBufferWorldNormal	 : SV_Target1; 
	float4 gBufferDiffuse		 : SV_Target2;
	float4 gBufferSpecular		 : SV_Target3;
	float4 gBufferReflectivity	 : SV_Target4; 
};

PSOutput main(PSInput psInput)
{
	PSOutput psOutput;
	psOutput.gBufferWorldPos = float4(psInput.PosW, shininess);
	psOutput.gBufferWorldNormal   = float4(psInput.NormalW, ior);
	psOutput.gBufferDiffuse	   = float4(diffuse, 1.0f);
	psOutput.gBufferSpecular   = float4(specular, 1.0f);
	psOutput.gBufferReflectivity = float4(1.0f, 1.0f, 1.0f, 1.0f);
	return psOutput;
}