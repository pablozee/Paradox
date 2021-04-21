#include "Common.hlsl"

[shader("miss")]
void Miss(inout HitInfo hitInfo)
{
	hitInfo.shadedColourAndHitT = float4(1.f, 0.f, 0.f, 0.f);
}