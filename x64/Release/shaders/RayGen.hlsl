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

	if (gBufDiffuse.w != 1.0f)
	{
		colour = float3(0.33f, 0.66f, 0.33f);

	}
	else
	{
		DirectionalLight dirLight;
		dirLight.directionalLightDirection = float3(0.0f, 15.0f, 15.0f);
		dirLight.padding = 0.1f;
		dirLight.directionalLightColour = float3(1.0f, 1.0f, 1.0f);
		dirLight.padding1 = 0.1f;

	//	dirLight.directionalLightDirection = mul(dirLight.directionalLightDirection, (float3x3)view);

		RayDesc ray;
		ray.Origin = gBufWorldPos;
		ray.Direction = -dirLight.directionalLightDirection;
		ray.TMin = 0.1f;
		ray.TMax = 100.f;

		HitInfo shadowRayPayload;
		shadowRayPayload.shadedColourAndHitT = float4(1.f, 0.f, 0.f, 0.f);
	//	TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, ~0, 0, 0, 0, ray, shadowRayPayload);
	//	TraceRay(SceneBVH, RAY_FLAG_NONE, 1, 0, 1, 0, ray, shadowRayPayload);

	//	colour = (shadowRayPayload.isInShadow ? 0.05f : 1.0f) * CalculateDirectionalLightColourGBuffer(dirLight, eyePos, viewDir, gBufNormalizedNormal, gBufShininess, gBufDiffuse.xyz, gBufSpecular);
	//	if (true)
	//	{
	//	}
		colour = CalculateDirectionalLightColourGBuffer(dirLight, eyePos, viewDir, gBufNormalizedNormal, gBufShininess, gBufDiffuse.xyz, gBufSpecular);
	}

	RTOutput[LaunchIndex.xy] = float4(colour, 1.f);
}
