#include "Def.hlsli"

float4 main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID) : SV_Position
{
    float2 anchor = PointPositions[instanceID];
    float value = PointValues[instanceID];
    float prevValue = PointPreviousValues[instanceID];
    
    float2 position = anchor, prevLen = prevValue * MaxLength, Len = value * MaxLength;
    float halfWidth = Width * 0.5;
    
    float h1 = max(prevLen, MinLength);
    float h2 = h1 - MinLength;
    float h3 = max(MinLength, Len - MinLength);
    
    float h[] = { h1, 0, h2, 0, h3, 0, 0 };
    
    if (vertexID & 1)
    {
        position.x += halfWidth;
        position.y -= h[vertexID - 1];
    }
    else
    {
        position.x -= halfWidth;
        position.y -= h[vertexID];
    }
    return mul(float4(position, 0, 1), TransformMatrix);
}