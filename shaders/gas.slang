struct VertexToFragment {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexToFragment vertMain(uint vertexID : SV_VertexID) {
  VertexToFragment output;

  output.uv = float2((vertexID << 1) & 2, vertexID & 2);
  output.position = float4(output.uv * 2.0f - 1.0f, 0.0f, 1.0f);

  return output;
}

[[vk::binding(0)]]
StructuredBuffer<float> gasDensityGrid;


[shader("fragment")]
float4 fragMain(VertexToFragment input) : SV_Target {
  int GRID_SIZE = 300;

    float2 gridPos = input.uv * float2(GRID_SIZE - 1, GRID_SIZE - 1);
    
    int x0 = int(floor(gridPos.x));
    int y0 = int(floor(gridPos.y));
    int x1 = min(x0 + 1, GRID_SIZE - 1);
    int y1 = min(y0 + 1, GRID_SIZE - 1);
    
    float tx = frac(gridPos.x);
    float ty = frac(gridPos.y);
    
    float d00 = gasDensityGrid[y0 * GRID_SIZE + x0];
    float d10 = gasDensityGrid[y0 * GRID_SIZE + x1];
    float d01 = gasDensityGrid[y1 * GRID_SIZE + x0];
    float d11 = gasDensityGrid[y1 * GRID_SIZE + x1];
    
    float d = lerp(lerp(d00, d10, tx), lerp(d01, d11, tx), ty);
    
    float norm = clamp(d * 0.08f, 0.0f, 1.0f); //0.03f

    if (norm < 0.005f)
    {
        discard;
    }

    float3 deepSpaceColor = float3(0.04f, 0.01f, 0.12f);
    float3 brightNebulaColor = float3(0.65f, 0.15f, 0.55f);
    float3 gasColor = lerp(deepSpaceColor, brightNebulaColor, norm);
    
    gasColor += pow(norm, 24.0f) * float3(0.4f, 0.7f, 1.0f);
    
    return float4(gasColor, norm * 0.5f);
}
