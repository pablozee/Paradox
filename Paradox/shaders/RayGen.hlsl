
#include "Common.hlsl"

[shader("raygeneration")]

void RayGen()
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;


	float3 sampledColour = float3(0.f, 0.f, 0.f);

	for (int i = 0; i < 8; i++)
	{
		float2 randomPixelLocation;
		randomPixelLocation.x = LaunchIndex.x + RandomFloat();
		randomPixelLocation.y = LaunchIndex.y + RandomFloat();
		float2 d = (((randomPixelLocation) / resolution.xy) * 2.f - 1.f);
		float aspectRatio = (resolution.x / resolution.y);

		// Setup the ray
		RayDesc ray;
		ray.Origin = viewOriginAndTanHalfFovY.xyz;
		ray.Direction = normalize((d.x * view[0].xyz * viewOriginAndTanHalfFovY.w * aspectRatio) - (d.y * view[1].xyz * viewOriginAndTanHalfFovY.w) + view[2].xyz);
		ray.TMin = 0.1f;
		ray.TMax = 1000.f;

		// Trace the ray
		HitInfo payload;
		payload.shadedColourAndHitT = float4(0.f, 0.f, 0.f, 0.f);

		TraceRay(
			SceneBVH,
			RAY_FLAG_NONE,
			0xFF,
			0,
			0,
			0,
			ray,
			payload);
		
		sampledColour += payload.shadedColourAndHitT.rgb;
	}
	
	float scale = 1.f / 8.f;

	sampledColour.r = sampledColour.r * scale;
	sampledColour.g = sampledColour.g * scale;
	sampledColour.b = sampledColour.b * scale;
	
	RTOutput[LaunchIndex.xy] = float4(sampledColour, 1.f);
}
