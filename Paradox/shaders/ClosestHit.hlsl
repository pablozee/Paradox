#include "Common.hlsl"

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	uint triangleIndex = PrimitiveIndex();
	float3 barycentrics = float3((1.0f - attrib.uv.x - attrib.uv.y), attrib.uv.x, attrib.uv.y);
	VertexAttributes vertex = GetVertexAttributes(triangleIndex, barycentrics);
	float3 colour = float3(0, 0, 0);
	float3 eyePos = float3(viewOriginAndTanHalfFovY.x, viewOriginAndTanHalfFovY.y, viewOriginAndTanHalfFovY.z);
	float3 normalizedNormal = normalize(vertex.normal);
	float3 viewDir = normalize(eyePos - barycentrics);

	if (useTex == 1)
	{
	//	int2 coord = floor(vertex.uv * textureResolution.x);
	//	colour = albedo.Load(int3(coord, 0)).rgb;
	} 
	else
	{
		/*
		for (int i = 0; i < numDirLights; i++)
		{
		//	colour += CalculateDirectionalLightColour(directionalLights[i], barycentrics, normalizedNormal, eyePos, viewDir);
		}
		
		for (int i = 0; i < numPointLights; i++)
		{
		//	colour += CalculatePointLightColour(pointLights[i], barycentrics, normalizedNormal, eyePos, viewDir);
		}
		*/
		float3 normalizedLightDirection = normalize(float3(0, 1, 0));
		float3 halfVec = normalize(normalizedLightDirection + viewDir);
		float  nDotL = dot(normalizedNormal, normalizedLightDirection);
		float  nDotH = dot(normalizedNormal, halfVec);
		float3 lambert = diffuse * max(nDotL, 0) * float3(1, 1, 1);
		float3 phong = specular * pow(max(nDotH, 0), shininess) * float3(1, 1, 1);
		colour = lambert + phong;
	}
	
	float3 finalColour =  colour ;

	payload.shadedColourAndHitT = float4(finalColour, RayTCurrent());
}