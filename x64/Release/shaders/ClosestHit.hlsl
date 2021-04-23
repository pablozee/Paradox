#include "Common.hlsl"

[shader("closesthit")]
void ClosestHit(inout HitInfo hitInfo, Attributes attrib)
{
	hitInfo.shadedColourAndHitT = float4(0.f, 0.f, 0.f, 0.f);
}