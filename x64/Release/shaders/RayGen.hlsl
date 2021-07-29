
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
		colour = float3(0.2f, 0.2f, 0.2f);
	}
	else
	{
		DirectionalLight dirLight;
		dirLight.directionalLightDirection = float3(0.0f, 30.0f, 0.0f);
		dirLight.padding = 0.1f;
		dirLight.directionalLightColour = float3(1.0f, 1.0f, 1.0f);
		dirLight.padding1 = 0.1f;
		
		RayDesc ray;
		ray.Origin = gBufWorldPos;
		ray.Direction = normalize(float3(0.0f, -30.0f, 0.0f));
		ray.TMin = 0.01;
		ray.TMax = 100000;

		ShadowPayload shadowPayload;

		// TraceRay(SceneBVH, 0, 0xFF, 0, 0, 0, ray, shadowPayload);

		float factor = shadowPayload.hit ? 0.1 : 1.0;

		colour = CalculateDirectionalLightColourGBuffer(dirLight, eyePos, viewDir, gBufNormalizedNormal, gBufShininess, gBufDiffuse.xyz, gBufSpecular);
	}

	RTOutput[LaunchIndex.xy] = float4(colour, 1.f);
}
