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
	matrix invWorld;
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
	matrix			 materialTransform;
}

cbuffer GBufferPassSceneCB : register(b2)
{
	matrix			 gBufferView;
	matrix			 proj;
	matrix			 invGBufView;
	matrix			 invProj;
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
	if (shininess != 0.f)
	{																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																									
	//	gBuffer.gBufferWorldPos.xyz = psInput.PosH.xyz / psInput.PosH.w;
	// UNCOMMENT	float4 invProjPosH = mul(float4(psInput.PosW, 1.0f), invProj);
	//	invProjPosH = mul(invProjPosH, invGBufView);
	//	UNCOMMENT gBuffer.gBufferWorldPos.xyz = invProjPosH.xyz / invProjPosH.w;
		gBuffer.gBufferWorldPos.xyz = psInput.PosW;
		gBuffer.gBufferWorldPos.w = ior;

		/*
		float4 homogNormalW = mul(float4(psInput.NormalW, 1.0f), invWorld);
		float3 deHomogNormalW = homogNormalW.xyz / homogNormalW.w;
		deHomogNormalW = normalize(deHomogNormalW);
		*/

		float4x4 worldView = invWorld;
//		worldView = mul(invWorld, gBufferView);

		float3x3 worldView3x3 = float3x3(worldView[0][0], worldView[0][1], worldView[0][2], worldView[1][0], worldView[1][1], worldView[1][2], worldView[2][0], worldView[2][1], worldView[2][2]);
		float3 worldNormalInvert = mul(psInput.NormalW, worldView3x3);
		worldNormalInvert = normalize(worldNormalInvert);

		gBuffer.gBufferWorldNormal.xyz = worldNormalInvert;
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