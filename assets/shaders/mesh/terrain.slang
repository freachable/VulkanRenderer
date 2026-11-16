// Terrain: the heightmap grid, textured with the top-down albedo map.

struct SceneUniforms
{
    float4x4 viewProj;
    float4   sunDirection;    // xyz: unit vector pointing towards the sun
    float4   cameraPosition;
};

[[vk::binding(0, 0)]] ConstantBuffer<SceneUniforms> scene;
[[vk::binding(1, 0)]] Sampler2D albedoMap;

struct VSInput
{
    float3 position;
    float3 normal;
    float2 uv;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal;
    float2 uv;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.position = mul(scene.viewProj, float4(input.position, 1.0));
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target
{
    float3 albedo = albedoMap.Sample(input.uv).rgb;
    float diffuse = saturate(dot(normalize(input.normal), scene.sunDirection.xyz));
    return float4(albedo * (0.35 + 0.65 * diffuse), 1.0);
}
