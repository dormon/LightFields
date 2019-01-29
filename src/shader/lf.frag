out vec4 fColor;
in vec2 vCoord;
in vec3 position;
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
uniform int kernel = 1;
uniform int depth = 0;
uniform float aspect = 1.f;
uniform uvec2 gridSize = uvec2(8,8);
layout(binding=0)uniform sampler2DArray tex;
layout(binding=1)uniform sampler2DArray texDepth;

vec3 planeLineInter(vec3 a, vec3 b, vec3 normal, vec3 p)
{
    vec3 direction = b-a;
    float u = dot(normal,p-a)/dot(normal,direction);
    return ((a + u*(direction)));
}

void main()
{
    vec4 c = vec4(0);
    float xSel;
    float ySel;
    if(mode <= 1)
    {
        if(mode==0)
        {
           xSel = clamp(xSelect,0,gridSize.x-1);
           ySel = clamp(ySelect,0,gridSize.y-1);
        }
        else
        {
            vec3 camPos = (inverse(view)*vec4(0,0,0,1)).xyz;
            vec3 centerRay = normalize(vec3(0,0,0) - camPos);
            float range = .2;
            float coox = (clamp(centerRay.x*-1,-range*aspect,range*aspect)/(range*aspect)+1)/2.;
            float cooy = (clamp(centerRay.y,-range,range)/range+1)/2.;
            xSel = clamp(coox*(gridSize.x-1),0,gridSize.x-1);
            ySel = clamp(cooy*(gridSize.y-1),0,gridSize.y-1);
        }
        c += texture(tex,vec3(vCoord,(floor(ySel)  )*gridSize.x+floor(xSel)  )) * (1-fract(xSel)) * (1-fract(ySel));
        c += texture(tex,vec3(vCoord,(floor(ySel)  )*gridSize.x+floor(xSel)+1)) * (  fract(xSel)) * (1-fract(ySel));
        c += texture(tex,vec3(vCoord,(floor(ySel)+1)*gridSize.x+floor(xSel)  )) * (1-fract(xSel)) * (  fract(ySel));
        c += texture(tex,vec3(vCoord,(floor(ySel)+1)*gridSize.x+floor(xSel)+1)) * (  fract(xSel)) * (  fract(ySel));
    }
    else if(mode <= 3)
    {
        xSel = vCoord.x*(gridSize.x-1);
        ySel = vCoord.y*(gridSize.y-1);

        vec3 camPos = (inverse(view)*vec4(0,0,0,1)).xyz;
        vec3 pixelPos = position;// (inverse(view)*inverse(proj)*vec4((gl_FragCoord.xy / winSize *2 -1)*far,far,far)).xyz;
        vec3 pixelRay = pixelPos - camPos;
        vec3 normal = vec3(0.0,0.0,1.0);

        vec3 planePoint = vec3(0.0,0.0,-focusDistance);
        vec2 intersCoord = planeLineInter(camPos, pixelPos, normal, planePoint).xy;
              
      intersCoord.y *= aspect;

        float zn = z*0.01;
        if(depth ==1)
        {
            float avgZ = 0.0;
            for (int i=0; i<4; i++)
            { 
                ivec2 neighbour = ivec2(floor(xSel) + i%2, floor(ySel)+i/2);
                int modulo = (i+1)%2;
                int division = i/2;
                float weight = (modulo+fract(xSel)*(1-modulo*2)) * (1-division+fract(ySel)*(-1+division*2));
                int slice = neighbour.y*int(gridSize.x)+neighbour.x;
       
                vec2 depthCoord = intersCoord;
                depthCoord += 0.5*scale;
                depthCoord /= scale;
                
                float kernel = xSelect;
                float texZ = 0.0;
                texZ = max(texture(texDepth,vec3(depthCoord,slice)).r, texZ);
                texZ = max(texture(texDepth,vec3(vec2(-kernel, -kernel)+depthCoord,slice)).r, texZ);
                texZ = max(texture(texDepth,vec3(vec2( kernel,  kernel)+depthCoord,slice)).r, texZ);
                texZ = max(texture(texDepth,vec3(vec2(-kernel,  kernel)+depthCoord,slice)).r, texZ);
                texZ = max(texture(texDepth,vec3(vec2( kernel, -kernel)+depthCoord,slice)).r, texZ);
                texZ = max(texture(texDepth,vec3(vec2( kernel, 0)+depthCoord,slice)).r, texZ);
                texZ = max(texture(texDepth,vec3(vec2( -kernel, 0)+depthCoord,slice)).r, texZ);
                texZ = max(texture(texDepth,vec3(vec2( 0, kernel)+depthCoord,slice)).r, texZ);
                texZ = max(texture(texDepth,vec3(vec2( 0, -kernel)+depthCoord,slice)).r, texZ);
                avgZ += texZ*weight;
                avgZ = clamp(avgZ/ySelect,0.0,1.0);
                //avgZ = min(texZ,avgZ);
                //avgZ = texture(texDepth,vec3(depthCoord,slice)).r;
            } 
                zn*=(avgZ);
        }

        if(mode == 2)
        {
            vec2 texCoord[4] = {vCoord, vCoord, vCoord, vCoord};
            for (int i=0; i<4; i++)
            {
                /*using the same points as color interpolation
                    C------D
                    |      |
                    |      |
                    A------B
                */
        
                ivec2 neighbour = ivec2(floor(xSel) + i%2, floor(ySel)+i/2);
                int modulo = (i+1)%2;
                int division = i/2;
                float weight = (modulo+fract(xSel)*(1-modulo*2)) * (1-division+fract(ySel)*(-1+division*2));
                int slice = neighbour.y*int(gridSize.x)+neighbour.x;
                
                texCoord[i] = intersCoord+scale*(vec2(xSel,ySel)-neighbour)*(zn/(1.0-zn));
                texCoord[i] += 0.5*scale;
                texCoord[i] /= scale;
                
                c += texture(tex,vec3(texCoord[i],slice)) * weight;
                //c += vec4(vec3(zn),1.0)*weight;
                //c = vec4(vec3(zn),1.0);
            }
        }
        else
        {
            ivec2 center = ivec2(round(xSel), round(ySel));
            float outDistance = length(vec2(xSel,ySel)-vec2(center+kernel+1));

            for(int x=center.x-kernel; x<=center.x+kernel; x++)
                for(int y=center.y-kernel; y<=center.y+kernel; y++)
                {                    
                    int slice = y*int(gridSize.x)+x;
                    if(x>(gridSize.x-1) || y>(gridSize.y-1) || x<0 || y<0 )
                        continue;
                    
                    vec2 texCoord = intersCoord+(vec2(xSel,ySel)-vec2(x,y))*(zn/(1.0-zn));
                    texCoord.x += 0.5*scale;
                    texCoord.y += 0.5*scale;
                    texCoord /= scale;
                    float weight = 1.0-(length(vec2(x,y)-vec2(xSel,ySel)))/(outDistance);
                    c += texture(tex, vec3(texCoord, slice)) * weight;
                }

                c /= c.w;
        }	
		//TODO react to changed position/orientation
        //TODO depth map based blur - DOF
    }
    fColor = c;
}
