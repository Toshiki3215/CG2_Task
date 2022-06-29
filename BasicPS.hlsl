#include "Basic.hlsli"

//0番スロットに設定されたテクスチャ
Texture2D<float4> tex : register(t0);

//0番スロットに設定されたサンプラー
SamplerState smp : register(s0);

float4 main(VSOutput input) :SV_TARGET
{
	// --- 拡散反射光(diffuse) --- //
	//右下奥　向きのライト
	//float3 light = normalize(float3(1, -1, 1));

	//光源へのベクトルと法線ベクトルの内積
	//float brightness = dot(-light, input.normal);

	//輝度をRGBに代入して出力
	//return float4(brightness,brightness,brightness,1);


	// --- 環境光(Ambient) --- //
	//右下奥　向きのライト
	float3 light = normalize(float3(1,-1,1));

	//diffuseを[0,1]の範囲にClampする
	float diffuse = saturate(dot(-light, input.normal));

	//アンビエント項を0.3として計算
	float brightness = diffuse + 0.3f;

	//輝度をRGBに代入して出力
	//return float4(brightness, brightness, brightness, 1);

	float4 texcolor = float4(tex.Sample(smp,input.uv));

	return float4(texcolor.rgb * brightness,texcolor.a) * color;

}
