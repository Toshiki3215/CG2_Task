float4 main(float4 pos : POSITION) : SV_POSITION
{
	return pos + float4(0.4f,0.4f,0,0);
}

//頂点シェーダ