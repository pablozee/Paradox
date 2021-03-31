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

cbuffer ObjectCB : register(b0)
{
	matrix			world;
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
	matrix			 materialTransform;
	float			 sheen;
	int				 useTex;
}

cbuffer GBufferPassSceneCB : register(b2)
{
	matrix			 view;
	matrix			 gBufferView;
	matrix			 proj;
}

struct PSInput
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float2 TexC : TEXCOORD;
	float3 NormalW : NORMAL;
};

struct GBuffer
{
	float4 gBufferWorldPos		 : SV_Target0;
	float4 gBufferWorldNormal	 : SV_Target1; 
	float4 gBufferDiffuse		 : SV_Target2;
	float4 gBufferSpecular		 : SV_Target3;
};

GBuffer main(PSInput psInput)
{
	GBuffer gBuffer;
	if (diffuse.x != 0.f && diffuse.y != 0.f && diffuse.z != 0.f)
	{
	//	GBuffer gBuffer;
		gBuffer.gBufferWorldPos.xyz = psInput.PosW;
		gBuffer.gBufferWorldPos.w = ior;
		gBuffer.gBufferWorldNormal.xyz = psInput.NormalW;
		gBuffer.gBufferWorldNormal.w = shininess;
		gBuffer.gBufferDiffuse.xyz = diffuse;
		gBuffer.gBufferDiffuse.w = 1.0f;
		gBuffer.gBufferSpecular.xyz = specular;
		gBuffer.gBufferSpecular.w = 1.0f;
	}
	else
	{
		gBuffer.gBufferWorldPos = float4(0.f, 0.f, 0.f, 0.f);
		gBuffer.gBufferWorldNormal = float4(0.f, 0.f, 0.f, 0.f);
		gBuffer.gBufferDiffuse = float4(0.f, 0.f, 0.f, 0.f);
		gBuffer.gBufferSpecular = float4(0.f, 0.f, 0.f, 0.f);
	}

	return gBuffer;
}