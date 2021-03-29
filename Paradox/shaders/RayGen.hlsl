
#include "Common.hlsl"

[shader("raygeneration")]

void RayGen()
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

	float3 colour = float3(0, 0, 0);

	float3 gBufWorldPos = gBufferWorldPos[LaunchIndex].xyz;
	float  gBufIOR = gBufferWorldPos[LaunchIndex].w;
	float3 gBufNormalizedNormal = normalize(gBufferNormal[LaunchIndex].xyz);
	float  gBufShininess = normalize(gBufferNormal[LaunchIndex].w);
	float3 gBufDiffuse = gBufferDiffuse[LaunchIndex].xyz;
	float3 gBufSpecular = gBufferSpecular[LaunchIndex].xyz;


	float3 eyePos = viewOriginAndTanHalfFovY.xyz;
	float3 viewDir = normalize(eyePos - gBufWorldPos);

	for (int i = 0; i < numDirLights; i++)
	{
		colour += CalculateDirectionalLightColourGBuffer(directionalLights[i], eyePos, viewDir, gBufNormalizedNormal,
														 gBufShininess, gBufDiffuse, gBufSpecular);
	}

	for (int x = 0; x < numPointLights; x++)
	{
		colour += CalculatePointLightColourGBuffer(pointLights[x],eyePos, viewDir, gBufWorldPos, gBufNormalizedNormal,
												   gBufShininess, gBufDiffuse, gBufSpecular);
	}
	
	RTOutput[LaunchIndex.xy] = float4(colour, 1.f);
}
