#include "Basic.hlsli"

//0�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`��
Texture2D<float4> tex : register(t0);

//0�ԃX���b�g�ɐݒ肳�ꂽ�T���v���[
SamplerState smp : register(s0);

float4 main(VSOutput input) :SV_TARGET
{
	//return float4(tex.Sample(smp,input.uv)) * float4(0,255,255,0);
	return float4(1,1,1,1);
}
