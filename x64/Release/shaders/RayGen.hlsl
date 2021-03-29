
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
	float  gBufShininess = gBufferNormal[LaunchIndex].w;
	float4 gBufDiffuse = gBufferDiffuse[LaunchIndex];
	float3 gBufSpecular = gBufferSpecular[LaunchIndex].xyz;


	float3 eyePos = viewOriginAndTanHalfFovY.xyz;
	float3 viewDir = normalize(eyePos - gBufWorldPos);

	/*	*/

	for (int i = 0; i < numDirLights; i++)
	{
		RayDesc ray;
		ray.Origin = viewOriginAndTanHalfFovY.xyz;
		ray.Direction = normalize(-directionalLights[i].directionalLightDirection);
		ray.TMin = 0.1f;
		ray.TMax = length(-directionalLights[i].directionalLightDirection);

		HitInfo payload;
		payload.shadedColourAndHitT = float4(0.f, 0.f, 0.f, 0.f);

		TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);

		colour += payload.shadedColourAndHitT.x * CalculateDirectionalLightColourGBuffer(directionalLights[i], eyePos, viewDir, gBufNormalizedNormal,
														 gBufShininess, gBufDiffuse.xyz, gBufSpecular);
	}

	for (int x = 0; x < numPointLights; x++)
	{
		RayDesc ray;
		ray.Origin = viewOriginAndTanHalfFovY.xyz;
		ray.Direction = normalize(pointLights[x].pointLightPosition - eyePos);
		ray.TMin = 0.1f;
		ray.TMax = length(pointLights[x].pointLightPosition - eyePos);

		HitInfo payload;
		payload.shadedColourAndHitT = float4(0.f, 0.f, 0.f, 0.f);

		TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);

		colour += payload.shadedColourAndHitT.x * CalculatePointLightColourGBuffer(pointLights[x], eyePos, viewDir, gBufWorldPos, gBufNormalizedNormal,
												   gBufShininess, gBufDiffuse.xyz, gBufSpecular);
	}
	/*
		*/
	RayDesc ray;
	ray.Origin = viewOriginAndTanHalfFovY.xyz;
	ray.Direction = normalize(-directionalLights[0].directionalLightDirection);
	ray.TMin = 0.1f;
	ray.TMax = length(-directionalLights[0].directionalLightDirection);

	HitInfo payload;
	payload.shadedColourAndHitT = float4(0.f, 0.f, 0.f, 0.f);

	TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload);


	colour = payload.shadedColourAndHitT.x * CalculateDirectionalLightColourGBuffer(directionalLights[0], eyePos, viewDir, gBufNormalizedNormal,
		gBufShininess, gBufDiffuse.xyz, gBufSpecular);
	
	if (gBufDiffuse.w == 0.0f)
	{
		colour = float3(0.2f, 0.2f, 0.2f);
	}

	RTOutput[LaunchIndex.xy] = float4(colour, 1.f);
}
