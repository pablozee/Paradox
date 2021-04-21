#include "Common.hlsl"

[shader("closesthit")]
void ClosestHit(inout HitInfo hitInfo, Attributes attrib)
{
	/*	
	uint triangleIndex = PrimitiveIndex();
	float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
	VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);
	float3 colour = float3(0, 0, 0);
	float3 eyePos = float3(viewOriginAndTanHalfFovY.x, viewOriginAndTanHalfFovY.y, viewOriginAndTanHalfFovY.z);
	float3 normalizedNormal = normalize(vertex.normal);
	float3 viewDir = normalize(eyePos - barycentrics);

	if (useTex == 1)
	{
		int2 coord = floor(vertex.uv * textureResolution.x);
	//	colour = albedo.Load(int3(coord, 0)).rgb;
	} 
	else
	{
		for (int i = 0; i < numDirLights; i++)
		{
			colour += CalculateDirectionalLightColour(directionalLights[i], barycentrics, normalizedNormal, eyePos, viewDir);
		}
		
		for (int i = 0; i < numPointLights; i++)
		{
			colour += CalculatePointLightColour(pointLights[i], barycentrics, normalizedNormal, eyePos, viewDir);
		}
	}
	float3 finalColour = ambient + colour + emission;
	*/
	

	//payload.shadedColourAndHitT = float4(0.f, 0.f, 0.f, RayTCurrent());

	hitInfo.shadedColourAndHitT = float4(1.f, 0.f, 0.f, 0.f);
}