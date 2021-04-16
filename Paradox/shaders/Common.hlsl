// Structures

struct HitInfo
{
	float4			 shadedColourAndHitT;
};

struct ShadowRayHitInfo
{
	bool isInShadow;
};

struct Attributes
{
	float2			 uv;
};

struct DirectionalLight
{
	float3			 directionalLightDirection;
	float			 padding;
	float3			 directionalLightColour;
	float			 padding1;
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
}

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
float3 CalculateDirectionalLightColourGBuffer(DirectionalLight light, float3 eyePos, float3 viewDir, 
											  float3 gBufNormalizedNormal, float  gBufShininess, float3 gBufDiffuse, float3 gBufSpecular)
{
	float3 normalizedLightDirection = normalize(light.directionalLightDirection.xyz);
	float3 halfVec = normalize(-normalizedLightDirection + viewDir);
	float  nDotL = dot(gBufNormalizedNormal, normalizedLightDirection);
	float  nDotH = dot(gBufNormalizedNormal, halfVec);
	float3 lambert = gBufDiffuse * max(nDotL, 0) * light.directionalLightColour.xyz;
//	float3 lambert = float3(0.7f, 0.0f, 0.2f) * max(nDotL, 0) * light.directionalLightColour.xyz;
	float3 phong = gBufSpecular * pow(max(nDotH, 0), gBufShininess) * light.directionalLightColour.xyz;
	return lambert;

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

float3 Fresnel(float3 f0, float cos_thetai)
{
	return f0 + (1 - f0) * pow(1 - cos_thetai, 5);
}

float3 CalculateGGXFValue(DirectionalLight directionalLight, float3 eyePos, float3 viewDir,
	float3 gBufNormalizedNormal, float gBufShininess, float3 gBufDiffuse, float3 gBufSpecular, float fresnel, float roughness)
{
	float3 normalizedLightDirection = normalize(-directionalLight.directionalLightDirection);
	float3 halfVec = normalizedLightDirection + viewDir;
	float  nDotL = dot(gBufNormalizedNormal, normalizedLightDirection);
	float  nDotH = dot(gBufNormalizedNormal, halfVec);

	float3 BRDF = 0;

	if (nDotL > 0 && nDotH > 0 && all(halfVec))
	{
		halfVec = normalize(halfVec);
		float matRoughness = roughness;
		float3 microfacetNormal = halfVec;

		float nDotM = saturate(dot(gBufNormalizedNormal, microfacetNormal));
		float hDotL = saturate(dot(halfVec, normalizedLightDirection));

		float denominator = 1 + nDotM * nDotM * (matRoughness * matRoughness - 1);
		float D = matRoughness * matRoughness / (denominator * denominator);

		float3 F = Fresnel(fresnel, hDotL);

		float G = 0.5 / lerp(2 * nDotL * nDotH, nDotL + nDotH, matRoughness);

		BRDF = F * G * D;
	}

	return BRDF;
}

float3 CalculateHammonFValue(float3 gBufDiffuse, float roughness, float3 gBufNormalizedNormal, float3 viewDir, DirectionalLight directionalLight, float fresnel)
{
	float3 diffuse = 0;

	float3 normalizedLightDirection = normalize(directionalLight.directionalLightDirection);


	float3 halfVec = normalizedLightDirection + viewDir;
	halfVec = normalize(halfVec);

	float nDotH = dot(gBufNormalizedNormal, halfVec);

	if (nDotH > 0)
	{
		float a = roughness * roughness;

		float nDotV = saturate(dot(gBufNormalizedNormal, viewDir));
		float nDotL = saturate(dot(gBufNormalizedNormal, normalizedLightDirection));
		float lDotV = saturate(dot(normalizedLightDirection, viewDir));

		float facing = 0.5 + 0.5 * lDotV;
		float rough = facing * (0.9 - 0.4 * facing) * ((0.5 + nDotH) / nDotH);
		float smooth = 1.0f * (1.0f - pow(1 - nDotL, 5)) * (1 - pow(1 - nDotV, 5));

		float3 single = lerp(smooth, rough, a);

		float multi = 0.3641 * a;

		diffuse = gBufDiffuse * (single + gBufDiffuse * multi);
	}

	return diffuse;

}

float3 CalculateShadedColour(DirectionalLight directionalLight, float3 eyePos, float3 viewDir,
	float3 gBufNormalizedNormal, float gBufShininess, float3 gBufDiffuse, float3 gBufSpecular, float fresnel, float roughness)
{
	float3 directLighting = 0;

	float3 normalizedLightDirection = normalize(-directionalLight.directionalLightDirection);
	float  nDotL = dot(gBufNormalizedNormal, normalizedLightDirection);

	if (nDotL > 0)
	{
		float3 directDiffuse = 0;

		directDiffuse = CalculateHammonFValue(gBufDiffuse, roughness, gBufNormalizedNormal, viewDir, directionalLight, fresnel);

		float3 directSpecular = 0;

		directSpecular = CalculateGGXFValue(directionalLight, eyePos, viewDir, gBufNormalizedNormal, gBufShininess, gBufDiffuse, gBufSpecular, fresnel, roughness);
		
		directLighting = nDotL * directionalLight.directionalLightColour * (directDiffuse + directSpecular);
	//	directLighting = nDotL * float3(1.0f, 0.0f, 0.0f) * (directDiffuse + directSpecular);
	}

	return directLighting;
}


