#include "Common.hlsl"

[shader("miss")]
void Miss(inout HitInfo payload)
{
	payload.shadedColourAndHitT.x = 1.f;
}