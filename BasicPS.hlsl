#include "Basic.hlsli"

//0番スロットに設定されたテクスチャ
Texture2D<float4> tex : register(t0);

//0番スロットに設定されたサンプラー
SamplerState smp : register(s0);

float4 main(VSOutput input) :SV_TARGET
{
	//RGBをそれぞれ法線のXYZ、Aを1で出力
	return float4(input.normal,1);
}
