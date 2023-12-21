#version 450

layout( location = 0 ) in vec2 inUV;

layout( location = 0 ) out vec4 outColor;

layout( binding = 0 ) uniform sampler2D inImage;

layout( push_constant ) uniform PushConstantsBlock
{
    int mWidth;
    int mHeight;
} PushConstants;

void main()
{
    vec3 color = texture( inImage, inUV ).rgb;

    float gamma = 1.0f;
    color = pow( color, vec3( 1.0 / gamma ) );
    outColor = vec4( color, 1.0f );
}