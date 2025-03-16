#include "C:\Users\user\Desktop\DirectX12MagicBook\src\BasicShaderHeader.hlsli"

struct Output
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD;
};

Output BasicVS( float4 pos : POSITION, float2 uv: TEXCOORD )
{
    Output output;
    output.svpos = pos;
    output.uv = uv;
    return output;
}