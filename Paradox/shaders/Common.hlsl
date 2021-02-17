// Structures

struct HitInfo
{
	float4 shadedColourAndHitT;
};

struct Attributes
{
	float2 uv;
};

// Constant Buffers

cbuffer ViewCB : register(b0)
{
	matrix view;
	float4 viewOriginAndTanHalfFovY;
	float2 resolution;
}

cbuffer MaterialCB : register(b1)
{
	float4 textureResolution;
}

// Resources

RWTexture2D<float4> RTOutput					: register(u0);
RaytracingAccelerationStructure SceneBVH		: register(t0);

ByteAddressBuffer indices						: register(t1);
ByteAddressBuffer vertices						: register(t2);
Texture2D<float4> albedo						: register(t3);

// Helper Functions

struct VertexAttributes
{
	float3 position;
	float2 uv;
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

	for (uint i = 0; i < 3; i++)
	{
		int address = (indices[i] * 5) * 4;
		// address is index times number of VertexAttributes individual data strucutres times the size of float 
		// Use this to index into vertices and retrieve the correct vertex and multiply by the barycentrics to get position
		v.position += asfloat(vertices.Load3(address)) * barycentrics[i];
		// Offset address by 12 bytes, to account for position float3 of 12 bytes
		address += (3 * 4);
		v.uv += asfloat(vertices.Load2(address)) * barycentrics[i];
	}

	return v;
}