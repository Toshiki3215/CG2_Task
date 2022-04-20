float4 main(float4 pos : POSITION) : SV_POSITION
{
	return pos + float4(0.5f,0.5f,0,0);
}