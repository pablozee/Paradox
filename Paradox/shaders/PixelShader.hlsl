struct PSInput
{
	float4 PosL : SV_POSITION;
	float2 TexC : TEXCOORD;
	float3 NormalL : NORMAL;
};

float4 main(PSInput psInput) : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}