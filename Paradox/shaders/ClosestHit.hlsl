#include "Common.hlsl"

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	payload.shadedColourAndHitT.x = 0.f;
}