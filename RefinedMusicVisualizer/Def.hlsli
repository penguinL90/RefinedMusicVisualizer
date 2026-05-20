cbuffer CBuffer : register(b0)
{
    float MaxLength;
    float MinLength;
    float Width;
    float Padding;
    float4 Color;
    matrix TransformMatrix;
}

StructuredBuffer<float> PointValues : register(t0);
StructuredBuffer<float> PointPreviousValues : register(t1);
StructuredBuffer<float2> PointPositions : register(t2);
