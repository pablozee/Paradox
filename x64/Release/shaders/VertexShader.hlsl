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
	float3x4 world3x4;
	float objPadding;
	matrix world;
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
	matrix gBufferView;
	matrix proj;
};

struct VSInput
{
	float3 Pos : POSITION;
	float2 TexC : TEXCOORD;
	float3 Normal : NORMAL;
};

struct VSOutput
{
	float4 PosH  : SV_POSITION;
	float3 PosW : POSITION;
	float2 TexCOut : TEXCOORD;
	float3 NormalW : NORMAL;
};

VSOutput main(VSInput vsInput)
{
	VSOutput vso;

	float4 homogPosW = mul(float4(vsInput.Pos, 1.0f), world);
	vso.PosW = homogPosW.xyz / homogPosW.w;

	vso.NormalW = vsInput.Normal;

	vso.TexCOut = vsInput.TexC;

	matrix viewProj = mul(gBufferView, proj);

	vso.PosH = mul(float4(vso.PosW, 1.0f), viewProj);

	return vso;
}

