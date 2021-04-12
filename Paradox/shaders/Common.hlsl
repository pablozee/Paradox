// Structures

struct HitInfo
{
	float4			 shadedColourAndHitT;
};

struct Attributes
{
	float2			 uv;
};

struct DirectionalLight
{
	float4			 directionalLightDirection;
	float4			 directionalLightColour;
};

struct PointLight
{
	float3			 pointLightPosition;
	float			 pointLightPadding;
	float3			 pointLightColour;
	float			 pointLightPadding1;
};

// Constant Buffers

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
	matrix			 materialTransform;
	float			 sheen;
	int				 useTex;
}

cbuffer GBufferPassSceneCB : register(b2)
{
	matrix			 gBufferView;
	matrix			 proj;
	matrix			 invGBufView;
	matrix			 invProj;
};

cbuffer RayTracingPassSceneCB : register(b3)
{
	float4x4		 view;
	float4			 viewOriginAndTanHalfFovY;
	float2			 resolution;
	float			 numDirLights;
	float			 numPointLights;
//	DirectionalLight	directionalLight;
}
/*
*/
cbuffer LightsBufferCB : register(b4)
{
	DirectionalLight	directionalLight;
}
// Resources

RWTexture2D<float4> RTOutput					: register(u0);
RaytracingAccelerationStructure SceneBVH		: register(t0);

ByteAddressBuffer indices						: register(t1);
ByteAddressBuffer vertices						: register(t2);
Texture2D<float4> gBufferWorldPos				: register(t3);
Texture2D<float4> gBufferNormal					: register(t4);
Texture2D<float4> gBufferDiffuse				: register(t5);
Texture2D<float4> gBufferSpecular				: register(t6);
//Texture2D<float4> albedo						: register(t3);

// Helper Functions

struct VertexAttributes
{
	float3 position;
	float2 uv;
	float3 normal;
};

uint3 GetIndices(uint triangleIndex)
{
	uint baseIndex = (triangleIndex * 3);
	int address = (baseIndex * 4);
	return indices.Load3(address);
}

VertexAttributes GetVertexAttributes(uint triangleIndex, float3 barycentrics)
{
	uint3 indices = GetIndices(triangleIndex);
	VertexAttributes v;
	v.position = float3(0, 0, 0);
	v.uv = float2(0, 0);
	v.normal = float3(0, 0, 0);

	for (uint i = 0; i < 3; i++)
	{
		int address = (indices[i] * 8) * 4;
		// address is index times number of VertexAttributes individual data strucutres times the size of float 
		// Use this to index into vertices and retrieve the correct vertex and multiply by the barycentrics to get position
		v.position += asfloat(vertices.Load3(address)) * barycentrics[i];
		// Offset address by 12 bytes, to account for position float3 of 12 bytes
		address += (3 * 4);
		v.uv += asfloat(vertices.Load2(address)) * barycentrics[i];
		address += (2 * 4);
		v.normal += asfloat(vertices.Load3(address)) * barycentrics[i];
	}

	return v;
}


/*
float RandomFloat()
{
	return frac(sin(dot(normalize(randomSeedVector0), normalize(randomSeedVector1)))) * 46146.1461f;
}
float3 CalculateDirectionalLightColour(DirectionalLight directionalLight, float3 barycentrics, float3 normalizedNormal, float3 eyePos, float3 viewDir)
{
	float3 normalizedLightDirection = normalize(directionalLight.directionalLightDirection);
	float3 halfVec = normalize(normalizedLightDirection + viewDir);
	float  nDotL = dot(normalizedNormal, normalizedLightDirection);
	float  nDotH = dot(normalizedNormal, halfVec);
	float3 lambert = diffuse * max(nDotL, 0) * directionalLight.directionalLightColour;
	float3 phong = specular * pow(max(nDotH, 0), shininess) * directionalLight.directionalLightColour;
	return lambert + phong;
}

float3 CalculatePointLightColour(PointLight pointLight, float3 barycentrics, float3 normalizedNormal, float3 eyePos, float3 viewDir)
{
	float3 normalizedLightDirection = normalize(pointLight.pointLightPosition - barycentrics);
	float3 halfVec = normalize(normalizedLightDirection + viewDir);
	float  nDotL = dot(normalizedNormal, normalizedLightDirection);
	float  nDotH = dot(normalizedNormal, halfVec);
	float3 lambert = diffuse * max(nDotL, 0) * pointLight.pointLightColour;
	float3 phong = specular * pow(max(nDotH, 0), shininess) * pointLight.pointLightColour;
	return lambert + phong;
}
*/
float3 CalculateDirectionalLightColourGBuffer(DirectionalLight directionalLight, float3 eyePos, float3 viewDir, 
											  float3 gBufNormalizedNormal, float  gBufShininess, float3 gBufDiffuse, float3 gBufSpecular)
{
	float3 normalizedLightDirection = normalize(-directionalLight.directionalLightDirection.xyz);
	float3 halfVec = normalize(normalizedLightDirection + viewDir);
	float  nDotL = dot(gBufNormalizedNormal, normalizedLightDirection);
	float  nDotH = dot(gBufNormalizedNormal, halfVec);
	float3 lambert = gBufDiffuse * max(nDotL, 0) * directionalLight.directionalLightColour.xyz;
//	float3 lambert = gBufDiffuse * max(nDotL, 0) * float3(0.1f, 0.1f, 0.9f);
	float3 phong = gBufSpecular * pow(max(nDotH, 0), gBufShininess) * directionalLight.directionalLightColour.xyz;
	return lambert + phong;

		//lambert
		//+ phong;
		// float3(1.f, 1.f, 0.f) + * max(nDotL, 0)lambert + phong;
}

float3 CalculatePointLightColourGBuffer(PointLight pointLight, float3 eyePos, float3 viewDir, float3 gBufWorldPos,
										float3 gBufNormalizedNormal, float  gBufShininess, float3 gBufDiffuse, float3 gBufSpecular)
{
	float3 normalizedLightDirection = normalize(pointLight.pointLightPosition - gBufWorldPos);
	float3 halfVec = normalize(normalizedLightDirection + viewDir);
	float  nDotL = dot(gBufNormalizedNormal, normalizedLightDirection);
	float  nDotH = dot(gBufNormalizedNormal, halfVec);
	float3 lambert = gBufDiffuse * max(nDotL, 0) * pointLight.pointLightColour;
	float3 phong = gBufSpecular * pow(max(nDotH, 0), gBufShininess) * pointLight.pointLightColour;
	return lambert + phong;
}


