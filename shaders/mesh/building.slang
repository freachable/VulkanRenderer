// Buildings: the vertex buffer holds flat footprint triangles; the geometry shader extrudes them
// into a roof plus one wall quad per outline edge. Faces are counter-clockwise seen from outside.

struct SceneUniforms
{
    float4x4 viewProj;
    float4   sunDirection;    // xyz: unit vector pointing towards the sun
    float4   cameraPosition;
};

[[vk::binding(0, 0)]] ConstantBuffer<SceneUniforms> scene;

struct VSInput
{
    float2 footprint;   // (x, z)
    float  baseHeight;
    float  roofHeight;
    float3 color;
    float  wallMask;    // 1: the edge from this vertex to the next one in the triangle gets a wall
};

struct GSInput
{
    float2 footprint;
    float  baseHeight;
    float  roofHeight;
    float3 color;
    float  wallMask;
};

struct GSOutput
{
    float4 position : SV_Position;
    float3 normal;
    float3 color;
};

[shader("vertex")]
GSInput vertMain(VSInput input)
{
    GSInput output;
    output.footprint = input.footprint;
    output.baseHeight = input.baseHeight;
    output.roofHeight = input.roofHeight;
    output.color = input.color;
    output.wallMask = input.wallMask;
    return output;
}

GSOutput makeVertex(float3 worldPosition, float3 normal, float3 color)
{
    GSOutput v;
    v.position = mul(scene.viewProj, float4(worldPosition, 1.0));
    v.normal = normal;
    v.color = color;
    return v;
}

// Roof (3) + up to three walls (4 each)
[shader("geometry")]
[maxvertexcount(15)]
void geomMain(triangle GSInput tri[3], inout TriangleStream<GSOutput> stream)
{
    float3 color = tri[0].color;

    // Roof: the input triangle lifted to roof height. Its winding already faces up.
    for (int i = 0; i < 3; i++)
    {
        float3 p = float3(tri[i].footprint.x, tri[i].roofHeight, tri[i].footprint.y);
        stream.Append(makeVertex(p, float3(0.0, 1.0, 0.0), color));
    }
    stream.RestartStrip();

    // Walls on outline edges. Edge a -> b runs counter-clockwise seen from above, so the strip
    // (aBottom, bBottom, aTop, bTop) faces outwards.
    for (int i = 0; i < 3; i++)
    {
        if (tri[i].wallMask < 0.5)
            continue;

        GSInput a = tri[i];
        GSInput b = tri[(i + 1) % 3];

        float3 aBottom = float3(a.footprint.x, a.baseHeight, a.footprint.y);
        float3 bBottom = float3(b.footprint.x, b.baseHeight, b.footprint.y);
        float3 aTop    = float3(a.footprint.x, a.roofHeight, a.footprint.y);
        float3 bTop    = float3(b.footprint.x, b.roofHeight, b.footprint.y);

        float3 normal = normalize(cross(bBottom - aBottom, aTop - aBottom));

        stream.Append(makeVertex(aBottom, normal, color));
        stream.Append(makeVertex(bBottom, normal, color));
        stream.Append(makeVertex(aTop, normal, color));
        stream.Append(makeVertex(bTop, normal, color));
        stream.RestartStrip();
    }
}

[shader("fragment")]
float4 fragMain(GSOutput input) : SV_Target
{
    float3 normal = normalize(input.normal);
    float diffuse = saturate(dot(normal, scene.sunDirection.xyz));
    // Roofs slightly brighter than walls, so outlines read well from above.
    float roofBoost = normal.y > 0.9 ? 1.1 : 1.0;
    return float4(input.color * (0.3 + 0.7 * diffuse) * roofBoost, 1.0);
}
