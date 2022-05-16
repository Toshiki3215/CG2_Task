cbuffer ConstBufferDataMaterial:register(b0)
{
	float4 color;  //êFÅ@(RGBA)
}

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
	return color;
}