#include "Common.hlsl"

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	uint triangleIndex = PrimitiveIndex();
	float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
	VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);
	float3 colour = float3(0, 0, 0);

	if (useTex == 1)
	{
		int2 coord = floor(vertex.uv * textureResolution.x);
		colour = albedo.Load(int3(coord, 0)).rgb;
	} 
	else
	{
		for (int i = 0; i < numDirLights; i++)
		{
			colour += CalculateDirectionalLightColour(directionalLights[i], barycentrics, vertex.normal);
		}
	}
	
	float3 finalColour = ambient + colour + emission;

	payload.shadedColourAndHitT = float4(finalColour, RayTCurrent());
}