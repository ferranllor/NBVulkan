struct Particle {
    float3 position;
    float3 velocity;
    float mass;
};

struct VertexInput {
    float3 position : POSITION;
    float3 velocity : VELOCITY;
    float mass : MASS;
};

struct VertexOutput {
    float3 color;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
    VertexOutput output;
    
    output.sv_position = float4(
    input.position.x * 0.5,
    input.position.y * 0.5,
    input.position.z,
    1.0
);
    
    float vSquare = dot(input.velocity, input.velocity);
    float kineticEnergy = 0.5 * input.mass * vSquare;

    // Adjust the division based on the scale of the energy
    float normalizedEnergy = clamp(kineticEnergy / 2500.0, 0.0, 1.0); 
    output.color = lerp(
    float3(0.0, 0.0, 1.0),
    float3(1.0, 0.0, 0.0),
    normalizedEnergy
);
    
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target {
    return float4(input.color, 1.0);
}
