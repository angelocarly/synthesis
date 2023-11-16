#version 450

layout( location = 0 ) in vec2 inUV;

layout( location = 0 ) out vec4 outColor;

layout( binding = 0 ) uniform usampler2D inImage;

vec3 pal( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return a + b*cos( 6.28318*(c*t+d) );
}

void main()
{
    int c = int( texture( inImage, inUV ).r );

    //[[0.500 0.500 0.500] [-0.412 0.848 -0.142] [0.888 1.000 1.000] [0.000 0.333 0.667]]
    float t = c / 10024.0f;
    vec3 color = t * pal( c / 10250.0f, vec3( .5f ), vec3( -.41, .8f, -.1f ), vec3( 0.8f, 1.f, 1.f ), vec3( 0.0f, 0.3f, 0.6f ) );

//    int red = c % 256;
//    int green = ( c / 256 ) % 256;
//    int blue = ( c / 256 / 256  ) % 256;
//    vec3 color = vec3( red / 255.0f, green / 255.0f, blue / 255.0f );
    outColor = vec4( color, 1.0f );
}