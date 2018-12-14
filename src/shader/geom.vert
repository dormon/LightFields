layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

uniform float aspect = 1.f;
uniform mat4 mvp = mat4(1.f);

out vec2 texCoord;
out vec3 normal;
out vec3 fragmentPosition;

void main()
{
	float size = 1.79;
	vec3 position = inPosition*size;
	position.y-=0.38;
	position.x *= aspect;
    gl_Position = mvp*vec4(position*size,1.0);

    texCoord = inTexCoord;
    normal = inNormal;
    fragmentPosition = inPosition;
}
