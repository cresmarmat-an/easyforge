#pragma once

// The renderer's own shaders, in HLSL for Direct3D 12.

namespace easyforge::internal::direct3d12
{
    // Every 2D shape is a quad drawn from four vertices, one instance per shape.
    // The pixel shader measures the distance to the shape's rounded outline,
    // which gives anti-aliased edges, rounded corners, and borders from one
    // formula. Colors come out multiplied by their alpha.
    inline constexpr const char* ShapeShader = R"hlsl(
cbuffer Frame : register(b0)
{
    float2 TargetSize;
};

cbuffer Choice : register(b1)
{
    uint SamplingIndex;
};

Texture2D Picture : register(t0);

// The four ways of sampling from the root signature. SamplingIndex picks one;
// it is the same for every pixel of a draw, so the branch costs nothing.
SamplerState LinearClamp : register(s0);
SamplerState LinearRepeat : register(s1);
SamplerState NearestClamp : register(s2);
SamplerState NearestRepeat : register(s3);

float4 SampleChosen(Texture2D picture, float2 coordinates)
{
    if (SamplingIndex == 1)
    {
        return picture.Sample(LinearRepeat, coordinates);
    }
    if (SamplingIndex == 2)
    {
        return picture.Sample(NearestClamp, coordinates);
    }
    if (SamplingIndex == 3)
    {
        return picture.Sample(NearestRepeat, coordinates);
    }
    return picture.Sample(LinearClamp, coordinates);
}

struct Instance
{
    float2 Origin : ORIGIN;
    float2 AxisX : AXIS_X;
    float2 AxisY : AXIS_Y;
    float2 Size : SIZE;
    float4 Coordinates : COORDINATES;
    float4 Fill : FILL;
    float4 Border : BORDER;
    float4 Shape : SHAPE;
    float4 Clip : CLIP;
};

struct Interpolated
{
    float4 Position : SV_Position;
    float2 Local : LOCAL;
    float2 Size : SIZE;
    float2 Coordinates : COORDINATES;
    nointerpolation float4 Fill : FILL;
    nointerpolation float4 Border : BORDER;
    nointerpolation float4 Shape : SHAPE;
    nointerpolation float4 Clip : CLIP;
    nointerpolation float4 Bounds : BOUNDS;
};

Interpolated VertexMain(Instance instance, uint vertex : SV_VertexID)
{
    // Corners in strip order, with a pixel of room around the shape for the
    // anti-aliased edge.
    float2 corner = float2(vertex & 1, vertex >> 1);
    float margin = instance.Shape.z == 2.0 ? 0.0 : 1.0;
    float2 local = lerp(-margin.xx, instance.Size + margin, corner);
    float2 pixel = instance.Origin + instance.AxisX * local.x + instance.AxisY * local.y;

    Interpolated output;
    output.Position = float4(pixel / TargetSize * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.Local = local;
    output.Size = instance.Size;
    float2 fraction = local / max(instance.Size, 0.0001);
    output.Coordinates = lerp(instance.Coordinates.xy, instance.Coordinates.zw, fraction);
    output.Fill = instance.Fill;
    output.Border = instance.Border;
    output.Shape = instance.Shape;
    output.Clip = instance.Clip;
    output.Bounds = float4(min(instance.Coordinates.xy, instance.Coordinates.zw), max(instance.Coordinates.xy, instance.Coordinates.zw));
    return output;
}

float RoundedBoxDistance(float2 position, float2 halfSize, float radius)
{
    float2 inside = abs(position) - halfSize + radius;
    return length(max(inside, 0.0)) + min(max(inside.x, inside.y), 0.0) - radius;
}

float4 Premultiply(float4 color)
{
    return float4(color.rgb * color.a, color.a);
}

float4 PixelMain(Interpolated input) : SV_Target
{
    float2 pixel = input.Position.xy;
    if (pixel.x < input.Clip.x || pixel.y < input.Clip.y || pixel.x >= input.Clip.z || pixel.y >= input.Clip.w)
    {
        discard;
    }

    float mode = input.Shape.z;
    if (mode == 2.0)
    {
        // A glyph: the atlas holds how much of each pixel the glyph covers.
        float coverage = Picture.Sample(NearestClamp, input.Coordinates).r;
        return Premultiply(input.Fill) * coverage;
    }

    float2 halfSize = input.Size * 0.5;
    float radius = min(input.Shape.x, min(halfSize.x, halfSize.y));
    float distance = RoundedBoxDistance(input.Local - halfSize, halfSize, radius);
    float coverage = saturate(0.5 - distance);

    float4 fill = Premultiply(input.Fill);
    if (mode == 1.0 || mode == 3.0)
    {
        float2 coordinates = clamp(input.Coordinates, input.Bounds.xy, input.Bounds.zw);
        float4 texel = SampleChosen(Picture, coordinates);
        // Mode 3 pictures already hold colors multiplied by alpha, so the tint is
        // multiplied too; plain pictures are tinted first and multiplied after.
        fill = mode == 3.0 ? texel * Premultiply(input.Fill) : Premultiply(texel * input.Fill);
    }

    float borderWidth = input.Shape.y;
    if (borderWidth > 0.0)
    {
        float inner = distance + borderWidth;
        fill = lerp(Premultiply(input.Border), fill, saturate(0.5 - inner));
    }
    return fill * coverage;
}
)hlsl";

    // Meshes lit by the sun and the light from everywhere, drawn into a
    // multisampled target. Lighting is worked out in linear light, and the result
    // goes back to sRGB.
    inline constexpr const char* MeshShader = R"hlsl(
cbuffer Object : register(b0)
{
    float4x4 World;
    float4x4 ViewProjection;
    float4x4 NormalMatrix;
    float4 BaseColor;
    float4 TowardSun;
    float4 SunColor;
    float4 Ambient;
    float4 CameraPosition;
    float4 Material;
};

cbuffer Choice : register(b1)
{
    uint SamplingIndex;
};

Texture2D BaseColorTexture : register(t0);

// The four ways of sampling from the root signature. SamplingIndex picks one;
// it is the same for every pixel of a draw, so the branch costs nothing.
SamplerState LinearClamp : register(s0);
SamplerState LinearRepeat : register(s1);
SamplerState NearestClamp : register(s2);
SamplerState NearestRepeat : register(s3);

float4 SampleChosen(Texture2D picture, float2 coordinates)
{
    if (SamplingIndex == 1)
    {
        return picture.Sample(LinearRepeat, coordinates);
    }
    if (SamplingIndex == 2)
    {
        return picture.Sample(NearestClamp, coordinates);
    }
    if (SamplingIndex == 3)
    {
        return picture.Sample(NearestRepeat, coordinates);
    }
    return picture.Sample(LinearClamp, coordinates);
}

struct Vertex
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 Coordinates : TEXCOORD;
};

struct Interpolated
{
    float4 Position : SV_Position;
    float3 WorldPosition : WORLD_POSITION;
    float3 Normal : NORMAL;
    float2 Coordinates : TEXCOORD;
};

Interpolated VertexMain(Vertex vertex)
{
    Interpolated output;
    float4 world = mul(World, float4(vertex.Position, 1.0));
    output.Position = mul(ViewProjection, world);
    output.WorldPosition = world.xyz;
    output.Normal = mul((float3x3)NormalMatrix, vertex.Normal);
    output.Coordinates = vertex.Coordinates;
    return output;
}

float3 ToLinear(float3 color)
{
    return color <= 0.04045 ? color / 12.92 : pow((color + 0.055) / 1.055, 2.4);
}

float3 ToSrgb(float3 color)
{
    color = saturate(color);
    return color <= 0.0031308 ? color * 12.92 : 1.055 * pow(color, 1.0 / 2.4) - 0.055;
}

float4 PixelMain(Interpolated input, bool front : SV_IsFrontFace) : SV_Target
{
    float3 albedo = BaseColor.rgb;
    if (Material.x > 0.5)
    {
        albedo *= ToLinear(SampleChosen(BaseColorTexture, input.Coordinates).rgb);
    }

    float3 normal = normalize(input.Normal) * (front ? 1.0 : -1.0);
    float3 sun = normalize(TowardSun.xyz);
    float lit = saturate(dot(normal, sun));

    // A soft highlight that grows sharper as the surface gets smoother.
    float3 toCamera = normalize(CameraPosition.xyz - input.WorldPosition);
    float3 halfway = normalize(sun + toCamera);
    float smoothness = 1.0 - Material.y;
    float shininess = exp2(10.0 * smoothness + 1.0);
    float highlight = pow(saturate(dot(normal, halfway)), shininess) * smoothness * smoothness * lit;

    float3 color = albedo * (Ambient.rgb + SunColor.rgb * lit) + SunColor.rgb * highlight * 0.5;
    return float4(ToSrgb(color), 1.0);
}
)hlsl";
}
