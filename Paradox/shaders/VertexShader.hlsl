struct VSInput
{
	float3 PosL : POSITION;
	float2 TexC : TEXCOORD;
	float3 NormalL : NORMAL;
};

struct VSOutput
{
	float4 PosL : SV_POSITION;
	float2 TexC : TEXCOORD;
	float3 NormalL : NORMAL;
};

VSOutput main(VSInput vsInput)
{
	VSOutput vso;
	vso.PosL = float4(vsInput.PosL, 1.0f);
	vso.TexC = vsInput.TexC;
	vso.NormalL = vsInput.NormalL;

	return vso;
}