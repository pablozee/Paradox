#include "common.hlsl"

[shader("closesthit")]
void ShadowRayClosestHit(inout ShadowRayHitInfo hitInfo, Attributes attrib)
{
	hitInfo.isInShadow = true;
}
