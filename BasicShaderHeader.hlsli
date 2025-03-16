// 頂点シェーダからピクセルシェーダへのやり取り
struct Output
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD;
};