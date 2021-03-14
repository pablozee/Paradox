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
	float3 PosW : WorldPos;
	float2 TexC : TexCoordOut;
	float3 NormalW : WorldNormal;
};

struct GBuffer
{
	float4 gBufferWorldPos		 : SV_Target0;
	float4 gBufferWorldNormal	 : SV_Target1; 
	float4 gBufferDiffuse		 : SV_Target2;
	float4 gBufferSpecular		 : SV_Target3;
	float4 gBufferReflectivity	 : SV_Target4; 
};

GBuffer main(PSInput psInput)
{
	GBuffer gBuffer;

	gBuffer.gBufferWorldPos = float4(1.0f, 1.0f, 1.0f, 1.0f);
	gBuffer.gBufferWorldNormal = float4(1.0f, 1.0f, 1.0f, 1.0f);
	gBuffer.gBufferDiffuse = float4(1.0f, 1.0f, 1.0f, 1.0f);
	gBuffer.gBufferSpecular = float4(1.0f, 1.0f, 1.0f, 1.0f);
	gBuffer.gBufferReflectivity = float4(1.0f, 1.0f, 1.0f, 1.0f);

	return gBuffer;
}