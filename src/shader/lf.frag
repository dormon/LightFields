out vec4 fColor;
in vec2 vCoord;
uniform float xSelect = 0.f;
uniform float ySelect = 0.f;
uniform float focusDistance = 0.f;
uniform float z = 0.f;
uniform float far = 0.0;
uniform float scale = 1.0;
uniform uvec2 winSize = uvec2(0,0);
uniform mat4 view = mat4(0.0);
uniform mat4 proj = mat4(0.0);
uniform int mode = 0;
uniform float aspect = 1.f;
uniform uvec2 gridSize = uvec2(8,8);
layout(binding=0)uniform sampler2DArray tex;
layout(binding=1)uniform sampler2DArray texDepth;
vec3 planeLineInter(vec3 a, vec3 b, vec3 normal, vec3 p)
{
    vec3 direction = b-a;
    float u = dot(normal,p-a)/dot(normal,direction);
    return ((a + u*(direction))).xy;
}
void main()
{
    vec4 c = vec4(0);
    float xSel;
    float ySel;
    vec2 texCoord[4] = {vCoord, vCoord, vCoord, vCoord};
    if(mode==0)
    {
        xSel = clamp(xSelect,0,gridSize.x-1);
        ySel = clamp(ySelect,0,gridSize.y-1);
    }
    else if(mode ==1)
    {
        vec3 camPos = (inverse(view)*vec4(0,0,0,1)).xyz;
        vec3 centerRay = normalize(vec3(0,0,0) - camPos);
        float range = .2;
        float coox = (clamp(centerRay.x*-1,-range*aspect,range*aspect)/(range*aspect)+1)/2.;
        float cooy = (clamp(centerRay.y,-range,range)/range+1)/2.;
        xSel = clamp(coox*(gridSize.x-1),0,gridSize.x-1);
        ySel = clamp(cooy*(gridSize.y-1),0,gridSize.y-1);
    }
    else if(mode ==2)
    {
        xSel = vCoord.x*(gridSize.x-1);
        ySel = vCoord.y*(gridSize.y-1);

        vec3 camPos = (inverse(view)*vec4(0,0,0,1)).xyz;
        vec3 pixelPos = (inverse(view)*inverse(proj)*vec4((gl_FragCoord.xy / winSize *2 -1)*far,far,far)).xyz;
        vec3 pixelRay = pixelPos - camPos;
        float dist = camPos.z;//length(vec3(0,0,0) - camPos);
        vec3 normal = vec3(0.0,0.0,1.0);

        vec3 planePoint = vec3(0.0,0.0,-focusDistance);
        vec2 intersCoord = planeLineInter(camPos, pixelPos, normal, planePoint).xy;

		vec4 color = vec4(0.0);
		for (int i=0; i<4; i++)
		{		
			//float z = texture(texDepth, vec3(texCoord, slice)).r;
            /*using the same points as color interpolation
                C------D
                |      |
                |      |
                A------B
            */
			//float slice = (floor(ySel)+i/2)*gridSize.x+floor(xSel)+i%2;	
    
            vec2 neighbour = vec2(floor(xSel)*(1-i%2) + ceil(xSel)*(i%2), floor(ySel)*(1-i/2) + ceil(ySel)*(i/2) );
            float zn = z/100;
            texCoord[i] = intersCoord+(vec2(xSel,ySel)-neighbour)*(zn/(1-zn));

            texCoord[i] += 0.5*scale;
			texCoord[i].y *= aspect;
            texCoord[i] /= scale;
		}	
		//TODO react to changed position/orientation
    }
    c += texture(tex,vec3(texCoord[0],(floor(ySel)  )*gridSize.x+floor(xSel)  )) * (1-fract(xSel)) * (1-fract(ySel));
    c += texture(tex,vec3(texCoord[1],(floor(ySel)  )*gridSize.x+floor(xSel)+1)) * (  fract(xSel)) * (1-fract(ySel));
    c += texture(tex,vec3(texCoord[2],(floor(ySel)+1)*gridSize.x+floor(xSel)  )) * (1-fract(xSel)) * (  fract(ySel));
    c += texture(tex,vec3(texCoord[3],(floor(ySel)+1)*gridSize.x+floor(xSel)+1)) * (  fract(xSel)) * (  fract(ySel));
    fColor = c;
}
