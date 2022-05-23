#include "Basic.hlsli"

float4 main(VSOutput input) :SV_TARGET
{
	return float4(input.uv,0,1);
}

//float4 main(float4 pos : SV_POSITION) : SV_TARGET
//{
//	return color;
//}