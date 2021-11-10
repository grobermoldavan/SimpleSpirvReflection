#version 450

struct InputGeometryVertex
{
    vec3 positionLS;
    vec3 normalLS;
    vec2 uv;
};

struct InputInstanceData
{
    mat4x4 transformWS;
};

layout(set = 0, binding = 0) uniform ViewProjection
{
    mat4x4 mat;
} al_viewProjection;

layout(set = 1, binding = 0) readonly buffer InputGeometry
{
    InputGeometryVertex vertices[];
} al_inputGeometry;

layout(set = 1, binding = 1) readonly buffer InputInstances
{
    InputInstanceData instanceData[];
} inputInstances;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;

void main() {
    gl_Position = vec4(0);
    out_position = vec3(0);
    out_normal = vec3(0);
    out_uv = vec2(0);
}
