#include "Basic.hlsli"

//0�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`��
Texture2D<float4> tex : register(t0);

//0�ԃX���b�g�ɐݒ肳�ꂽ�T���v���[
SamplerState smp : register(s0);

float4 main(VSOutput input) :SV_TARGET
{
	//RGB�����ꂼ��@����XYZ�AA��1�ŏo��
	return float4(input.normal,1);
}
