#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec3 out_position;
layout(location = 2) out vec3 out_normal;

layout(input_attachment_index = 3, set = 0, binding = 1) uniform subpassInput testSubpassInput;
layout(set = 0, binding = 0) uniform sampler2D testSamplerArray[10];
layout(set = 0, binding = 2) readonly buffer testVerticesBuffer
{
    int vertices[];
} inputGeometry[120];

void main()
{

}
