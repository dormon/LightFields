out vec4 color;
in vec3 normal;
in vec2 texCoord;
in vec3 fragmentPosition;

layout(binding=2)uniform sampler2D texSampler;

void main()
{
    vec3 lightColor = vec3(0.9,0.9,1.0);
    vec3 lightDirection = normalize(vec3(10.0,5.0,10.0) - fragmentPosition);
    float ambient = 0.2;
    float diffuse = max(0.0, dot(normalize(normal), lightDirection));

    vec4 textureColor = texture(texSampler,texCoord);
    color = textureColor*vec4(lightColor*(ambient+diffuse), 1.0);
}