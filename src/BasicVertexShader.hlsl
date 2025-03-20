#include "C:\Users\user\Desktop\DirectX12MagicBook\src\BasicShaderHeader.hlsli"

struct Output
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer cbuff0 : register(b0)
{
    matrix mat; // ïœä∑çsóÒ
};

Output BasicVS( float4 pos : POSITION, float2 uv: TEXCOORD )
{
    Output output;
    output.svpos = mul(mat, pos);
    output.uv = uv;
    return output;
}