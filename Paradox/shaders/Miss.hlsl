#include "Common.hlsl"

[shader("miss")]
void Miss(inout ShadowPayload shadowPayload)
{
	shadowPayload.hit = false;
}