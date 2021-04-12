
#include "Common.hlsl"

[shader("raygeneration")]

void RayGen()
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

	float3 colour = float3(0, 0, 0);

	float3 gBufWorldPos = gBufferWorldPos[LaunchIndex].xyz;
	float  gBufIOR = gBufferWorldPos[LaunchIndex].w;
	float3 gBufNormalizedNormal = gBufferNormal[LaunchIndex].xyz;
	float  gBufShininess = gBufferNormal[LaunchIndex].w;
	float4 gBufDiffuse = gBufferDiffuse[LaunchIndex];
	float3 gBufSpecular = gBufferSpecular[LaunchIndex].xyz;


	float3 eyePos = viewOriginAndTanHalfFovY.xyz;
	float3 viewDir = normalize(eyePos - gBufWorldPos);

	/*	


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
	
	RayDesc ray;
	ray.Origin = gBufWorldPos;
	ray.Direction = normalize(directionalLight.directionalLightDirection);
	ray.TMin = 0.1f;
	ray.TMax = length(directionalLight.directionalLightDirection);

	HitInfo payload;
	payload.shadedColourAndHitT = float4(0.f, 0.f, 0.f, 0.f);

	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

		*/
	if (gBufDiffuse.w != 1.0f)
	{
		colour = float3(0.2f, 0.2f, 0.2f);

	}
	else
	{
		DirectionalLight dirLight;
		dirLight.directionalLightDirection = float3(0.0f, -15.0f, -10.0f);
		dirLight.padding = 0.1f;
		dirLight.directionalLightColour = float3(0.33f, 0.33f, 0.33f);
		dirLight.padding1 = 0.1f;


	//	colour = CalculateDirectionalLightColourGBuffer(directionalLight, eyePos, viewDir, gBufNormalizedNormal, gBufShininess, gBufDiffuse.xyz, gBufSpecular);
		colour = CalculateShadedColour(directionalLight, eyePos, viewDir, gBufNormalizedNormal, gBufShininess, gBufDiffuse.xyz, gBufSpecular, 0.1f, 0.1f);
	}

	/*
	float3 normalizedLightDirection = normalize(directionalLight.directionalLightDirection.xyz);
	float  nDotL = dot(gBufNormalizedNormal, normalizedLightDirection);
	float3 lambert = gBufDiffuse * max(nDotL, 0) * directionalLight.directionalLightColour.xyz;
	colour = lambert;



	*/

	RTOutput[LaunchIndex.xy] = float4(colour, 1.f);
}
