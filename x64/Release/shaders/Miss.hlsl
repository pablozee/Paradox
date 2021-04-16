#include "Common.hlsl"

[shader("miss")]
void ShadowRayMiss(inout ShadowRayHitInfo hitInfo)
{
	hitInfo.isInShadow = false;
}