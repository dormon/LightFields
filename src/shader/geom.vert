layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexcoord;

uniform float aspect;
uniform mat4 mvp;

out vec2 texCoord;
out vec3 normal;
out vec3 fragmentPosition;

void main()
{
    gl_Position = mvp*vec4(inPosition,1.0);
    gl_Position.x *= aspect;

    texCoord = inTexcoord;
    normal = inNormal;
    fragmentPosition = inPosition;
}
