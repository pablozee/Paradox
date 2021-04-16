#include "Common.hlsl"

[shader("closesthit")]
void ShadowRayClosestHit(inout ShadowRayHitInfo hitInfo, Attributes bary)
{
	hitInfo.isInShadow = true;
}

[shader("miss")]
void ShadowRayMiss(inout ShadowRayHitInfo hitInfo : SV_RayPayload)
{
	hitInfo.isInShadow = false;
}