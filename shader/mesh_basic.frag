#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inWorldNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inMaterialId;

layout(location = 0) out vec4 outColor;

struct Material {
    int textureId;
    float roughness;
    float metallic;
    float pad0;
    float pad1;
};

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    Material materials[];
} materialBuffer;

layout(set = 0, binding = 4) uniform sampler2D textures[];

void main() {
    Material material = materialBuffer.materials[inMaterialId];

    vec4 texColor = vec4(1.0);
    if (material.textureId >= 0) {
        texColor = texture(textures[nonuniformEXT(material.textureId)], inTexCoord);
    }

    vec3 lightDir = normalize(vec3(0.4, 0.7, 0.2));
    float ndotl = max(dot(normalize(inWorldNormal), lightDir), 0.1);

    outColor = vec4(inColor.rgb * texColor.rgb * ndotl, inColor.a * texColor.a);
}
