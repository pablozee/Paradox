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

// Constant Buffers

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

// Resources

RWTexture2D<float4> RTOutput					: register(u0);
RaytracingAccelerationStructure SceneBVH		: register(t0);

ByteAddressBuffer indices						: register(t1);
ByteAddressBuffer indices2						: register(t2);
ByteAddressBuffer vertices						: register(t3);
ByteAddressBuffer vertices2						: register(t4);
Texture2D<float4> albedo						: register(t5);

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
	}
	return v;
}


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
