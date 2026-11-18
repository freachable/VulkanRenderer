# Render flow and pipelines (proposal)

> Sketch by P1 for discussion, as asked in the group. Struct names for splats are from P2's message.

## One frame

```
CPU   update camera -> FrameUniforms (view, proj, cameraPos, viewport)          [P4]
      cull terrain chunks + buildings against the frustum                       [P1]

GPU   compute pass (no render target)
        1. splat projection: Gaussian3D -> ProjectedSplat2D, frustum/alpha cull [P2]
        2. sort visible splats by depth (radix sort)                            [P2]
        3. write DrawIndirectCommand (instanceCount = visible splats)           [P2]
      barrier: compute writes -> vertex/indirect reads

      graphics pass (dynamic rendering, color + depth)
        4. opaque:  terrain, buildings      depth test + write                  [P1]
        5. splats:  billboards, back to front, depth test, no depth write,
                    alpha blending over the meshes = compositing               [P3]
      present                                                                   [P4]
```

Splats are drawn after the meshes and test against their depth, so buildings hide splats behind
them, and splats in front blend over the buildings.

## Pipelines

| Pipeline | Owner | Stages | Input | Depth | Cull | Blend | Draw |
|---|---|---|---|---|---|---|---|
| terrain | P1 | vert, frag | vertex buffer (pos, normal, uv), albedo texture | test + write | back | off | indexed, per visible chunk |
| buildings | P1 | vert, frag (later + geom / mesh shader) | vertex buffer (pos, normal, color) | test + write | back | off | indexed, per visible building |
| splat projection | P2 | compute | SSBO Gaussian3D -> SSBO ProjectedSplat2D | - | - | - | dispatch |
| splat sort | P2 | compute | SSBO keys/indices | - | - | - | dispatch |
| splat billboards | P3 | vert, frag | no vertex buffer: SSBO ProjectedSplat2D + sorted indices | test, no write | none | premultiplied alpha | indirect |

All pipelines read the camera from the same uniform buffer in descriptor set 0. Each owner keeps
their own buffers, textures and descriptor set 1.

## What the framework would need

- A hook per owner, called by the Renderer every frame:

  ```cpp
  class RenderFeature   // e.g. MapRenderer (P1), SplatRenderer (P2 + P3)
  {
  public:
      virtual void update(Frame const&, Camera const&) {}                         // CPU work, uploads
      virtual void recordCompute(vk::raii::CommandBuffer const&, Frame const&) {} // before the graphics pass
      virtual void recordOpaque(vk::raii::CommandBuffer const&, Frame const&) {}  // depth write on
      virtual void recordBlended(vk::raii::CommandBuffer const&, Frame const&) {} // after opaque, depth read-only
  };
  ```

  The Renderer owns the frame: uniforms, barriers between the compute and graphics pass,
  begin/end rendering, and calls the features in this order.
- `GraphicsPipelineConfig`: depth write on/off, blend mode, pipelines without vertex input.
- A compute pipeline class and storage buffers (SSBO), plus indirect draws.
- Device features: `geometryShader` (P1, later), `drawIndirectCount` (P1 GPU culling, maybe P2).

## Open questions

1. Is a `RenderFeature` interface like the one above fine for everyone, or do we prefer something else?
2. Splats and meshes: do landmark splats need to write depth (e.g. for shadows), or is depth test only enough?
3. Who does the barriers: the Renderer between the passes (proposed), or each feature itself?
