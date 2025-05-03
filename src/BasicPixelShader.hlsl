#include "BasicShaderHeader.hlsli"

struct Output
{
    float4 svpos : SV_POSITION;
    float4 normal : NORMAL;
    float2 uv : TEXCOORD;
};

Texture2D<float4> tex : register(t0); //0番スロットに設定されたテクスチャ
SamplerState smp : register(s0); //0番スロットに設定されたサンプラ

//定数バッファ
cbuffer cbuff0 : register(b0)
{
    matrix world; //ワールド変換行列
    matrix viewproj; //ビュープロジェクション行列
};

float4 BasicPS(Output input) : SV_TARGET
{
    float3 light = normalize(float3(1, -1, 1));
    float brightness = dot(-light, input.normal);
    return float4(brightness, brightness, brightness, 1);
	//return float4(input.normal.xyz,1);
	//return float4(tex.Sample(smp,input.uv));
}