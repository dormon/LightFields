#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_ARB_bindless_texture : require

#define LF_SIZE 100

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
uniform int frame = 1;
uniform int depth = 0;
uniform int printStats = 0;
uniform float aspect = 1.f;
uniform uvec2 gridSize = uvec2(8,8);
layout(binding=0)uniform sampler2DArray tex;
layout(binding=1)uniform sampler2DArray texDepth;

layout(bindless_sampler)uniform sampler2D lfTextures[LF_SIZE];

layout(std430, binding = 2) buffer statisticsLayout
{
   uint statistics[];
};

void incStatPixel(ivec2 cam, vec2 pix)
{
    const uint width = 1920;
    const uint height = 1080;
    pix=clamp(pix, 0.0, 1.0)*vec2(width,height);
    uint imageSize = width*height;
    uvec2 pixDown = uvec2(floor(pix.x), floor(pix.y));
    uvec2 pixUp = uvec2(ceil(pix.x), ceil(pix.y));
    uint camIndex = imageSize*(cam.y*gridSize.x+cam.x);
    uint pixIndex = camIndex+pixDown.y*width+pixDown.x;
    atomicAdd(statistics[pixIndex],1);
    pixIndex = camIndex+pixDown.y*width+pixUp.x;
    atomicAdd(statistics[pixIndex],1);
    pixIndex = camIndex+pixUp.y*width+pixUp.x;
    atomicAdd(statistics[pixIndex],1);
    pixIndex = camIndex+pixUp.y*width+pixDown.x;
    atomicAdd(statistics[pixIndex],1);
}

vec3 planeLineInter(vec3 a, vec3 b, vec3 normal, vec3 p)
{
    vec3 direction = b-a;
    float u = dot(normal,p-a)/dot(normal,direction);
    return ((a + u*(direction)));
}

vec3 rgb2xyz(vec3 c){
    vec3 tmp=vec3(
        (c.r>.04045)?pow((c.r+.055)/1.055,2.4):c.r/12.92,
        (c.g>.04045)?pow((c.g+.055)/1.055,2.4):c.g/12.92,
        (c.b>.04045)?pow((c.b+.055)/1.055,2.4):c.b/12.92
    );
    mat3 mat=mat3(
        .4124,.3576,.1805,
        .2126,.7152,.0722,
        .0193,.1192,.9505
    );
    return 100.*(tmp*mat);
}
vec3 xyz2laba(vec3 c){
    vec3 n=c/vec3(95.047,100.,108.883),
         v=vec3(
        (n.x>.008856)?pow(n.x,1./3.):(7.787*n.x)+(16./116.),
        (n.y>.008856)?pow(n.y,1./3.):(7.787*n.y)+(16./116.),
        (n.z>.008856)?pow(n.z,1./3.):(7.787*n.z)+(16./116.)
    );
    return vec3((116.*v.y)-16.,500.*(v.x-v.y),200.*(v.y-v.z));
}
vec3 rgb2lab(vec3 c){
    vec3 lab=xyz2laba(rgb2xyz(c));
    return vec3(lab.x/100.,.5+.5*(lab.y/127.),.5+.5*(lab.z/127.));
}


const float PI = 3.141592653589793238462643;
vec3 bgr2xyz(vec3 BGR)
{
    float r = BGR[0];
    float g = BGR[1];
    float b = BGR[2];
    if( r > 0.04045 )
        r = pow( ( r + 0.055 ) / 1.055, 2.4 );
    else
        r = r / 12.92;
    if( g > 0.04045 )
        g = pow( ( g + 0.055 ) / 1.055, 2.4 );
    else
        g = g / 12.92;
    if( b > 0.04045 )
        b = pow( ( b + 0.055 ) / 1.055, 2.4 );
    else
        b = b / 12.92;
    r *= 100.0;
    g *= 100.0;
    b *= 100.0;
    return vec3(r * 0.4124 + g * 0.3576 + b * 0.1805,
    r * 0.2126 + g * 0.7152 + b * 0.0722,
    r * 0.0193 + g * 0.1192 + b * 0.9505);
}

vec3 xyz2lab(vec3 XYZ)
{
    float x = XYZ[0] / 95.047;
    float y = XYZ[1] / 95.047;
    float z = XYZ[2] / 95.047;
    if( x > 0.008856 )
        x = pow( x , .3333333333 );
    else
        x = ( 7.787 * x ) + ( 16.0 / 116.0 );
    if( y > 0.008856 )
        y = pow( y , .3333333333 );
    else
        y = ( 7.787 * y ) + ( 16.0 / 116.0 );
    if( z > 0.008856 )
        z = pow( z , .3333333333 );
    else
        z = ( 7.787 * z ) + ( 16.0 / 116.0 );
    return vec3(
    ( 116.0 * y ) - 16.0,
    500.0 * ( x - y ),
    200.0 * ( y - z ));
}

vec3 lab2lch(vec3 Lab)
{
    return vec3(
    Lab[0],
    sqrt( ( Lab[1] * Lab[1] ) + ( Lab[2] * Lab[2] ) ),
    atan( Lab[1], Lab[2] ));
}

float deltaE2000l( vec3 lch1, vec3 lch2 )
{
    float avg_L = ( lch1[0] + lch2[0] ) * 0.5;
    float delta_L = lch2[0] - lch1[0];
    float avg_C = ( lch1[1] + lch2[1] ) * 0.5;
    float delta_C = lch1[1] - lch2[1];
    float avg_H = ( lch1[2] + lch2[2] ) * 0.5;
    if( abs( lch1[2] - lch2[2] ) > PI )
        avg_H += PI;
    float delta_H = lch2[2] - lch1[2];
    if( abs( delta_H ) > PI )
    {
        if( lch2[2] <= lch1[2] )
            delta_H += PI * 2.0;
        else
            delta_H -= PI * 2.0;
    }

    delta_H = sqrt( lch1[1] * lch2[1] ) * sin( delta_H ) * 2.0;
    float T = 1.0 -
            0.17 * cos( avg_H - PI / 6.0 ) +
            0.24 * cos( avg_H * 2.0 ) +
            0.32 * cos( avg_H * 3.0 + PI / 30.0 ) -
            0.20 * cos( avg_H * 4.0 - PI * 7.0 / 20.0 );
    float SL = avg_L - 50.0;
    SL *= SL;
    SL = SL * 0.015 / sqrt( SL + 20.0 ) + 1.0;
    float SC = avg_C * 0.045 + 1.0;
    float SH = avg_C * T * 0.015 + 1.0;
    float delta_Theta = avg_H / 25.0 - PI * 11.0 / 180.0;
    delta_Theta = exp( delta_Theta * -delta_Theta ) * ( PI / 6.0 );
    float RT = pow( avg_C, 7.0 );
    RT = sqrt( RT / ( RT + 6103515625.0 ) ) * sin( delta_Theta ) * -2.0; // 6103515625 = 25^7
    delta_L /= SL;
    delta_C /= SC;
    delta_H /= SH;
    return sqrt( delta_L * delta_L + delta_C * delta_C + delta_H * delta_H + RT * delta_C * delta_H );
}
float deltaE2000( vec3 bgr1, vec3 bgr2 )
{
    vec3 xyz1, xyz2, lab1, lab2, lch1, lch2;
    xyz1 = bgr2xyz( bgr1);
    xyz2 = bgr2xyz( bgr2);
    lab1 = xyz2lab( xyz1);
    lab2 = xyz2lab( xyz2);
    lch1 = lab2lch( lab1);
    lch2 = lab2lch( lab2);
    return deltaE2000l( lch1, lch2 );
}
//vec3 rgb2lab(vec3 c) {return c;}

vec4 blurSample(int slice, int kernel, vec2 coords)
{
    const float DELTA = 0.001;
    vec4 color = vec4(0);
    for (int x=-kernel; x<=kernel; x++)
        for (int y=-kernel; y<=kernel; y++)
            color += texture(lfTextures[slice],coords+vec2(x,y)*DELTA);

    return color/pow((2*kernel+1),2);
}

void main()
{
    vec4 c = vec4(0);
    float xSel;
    float ySel;
    ivec2 gridIndex = ivec2(gridSize.x-1, gridSize.y-1);
        
    if(mode <= 1)
    {
        if(mode==0)
        {
           xSel = clamp(xSelect,0,gridIndex.x);
           ySel = clamp(ySelect,0,gridIndex.y);
           //if(printStats == 1)
           //incStatPixel(ivec2(xSel, ySel), vCoord);
        }
        else
        {
            vec3 camPos = (inverse(view)*vec4(0,0,0,1)).xyz;
            vec3 centerRay = normalize(vec3(0,0,0) - camPos);
            float range = .2;
            float coox = (clamp(centerRay.x*-1,-range*aspect,range*aspect)/(range*aspect)+1)/2.;
            float cooy = (clamp(centerRay.y,-range,range)/range+1)/2.;
            xSel = clamp(coox*(gridIndex.x),0,gridIndex.x);
            ySel = clamp(cooy*(gridIndex.y),0,gridIndex.y);
        }
        uint offset = gridSize.x*gridSize.y*frame;
        c += texture(lfTextures[int(offset + (floor(ySel)  )*gridSize.x+floor(xSel) )],vCoord) * (1-fract(xSel)) * (1-fract(ySel));
        c += texture(lfTextures[int(offset + (floor(ySel)  )*gridSize.x+floor(xSel)+1)],vCoord) * (  fract(xSel)) * (1-fract(ySel));
        c += texture(lfTextures[int(offset + (floor(ySel)+1)*gridSize.x+floor(xSel)  )],vCoord) * (1-fract(xSel)) * (  fract(ySel));
        c += texture(lfTextures[int(offset + (floor(ySel)+1)*gridSize.x+floor(xSel)+1)],vCoord) * (  fract(xSel)) * (  fract(ySel));
    }
    else if(mode > 1)
    {
        xSel = vCoord.x*(gridIndex.x);
        ySel = vCoord.y*(gridIndex.y);

        vec3 camPos = (inverse(view)*vec4(0,0,0,1)).xyz;
        vec3 pixelPos = position;// (inverse(view)*inverse(proj)*vec4((gl_FragCoord.xy / winSize *2 -1)*far,far,far)).xyz;
        vec3 pixelRay = pixelPos - camPos;
        vec3 normal = vec3(0.0,0.0,1.0);

        vec3 planePoint = vec3(0.0,0.0,-focusDistance);
        vec2 intersCoord = planeLineInter(camPos, pixelPos, normal, planePoint).xy;
              
      intersCoord.y *= aspect;

        float zn = z*0.01;
/*        if(depth ==1)
                zn = texture(texDepth,vec3(vec2(0.0), 0)).r;
*/

        if(mode == 2)
        {
            vec2 texCoord[4] = {vCoord, vCoord, vCoord, vCoord};

const float searchDelta = 0.001;
const int searchIterations = 50;
vec4 cols[searchIterations];
float znn = zn;
int s = (depth==1) ? 1 : searchIterations;
for(int j=0; j<s; j++)
{
zn=znn+(1.0-2*(j%2))*searchDelta*(j/2);
vec3 colors[4];
vec3 sum = vec3(0.0);

            c = vec4(0.0);
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
                int slice = int(gridSize.x*gridSize.y)*frame + (gridIndex.y-neighbour.y)*int(gridSize.x)+neighbour.x;
                
                texCoord[i] = intersCoord+scale*(vec2(xSel,ySel)-neighbour)*(zn/(1.0-zn));
                texCoord[i] += 0.5*scale;
                texCoord[i] /= scale;

colors[i] = (texture(lfTextures[slice], texCoord[i]).xyz);
sum += (colors[i].xyz); 

                c += texture(lfTextures[slice],texCoord[i]) * weight;
                if(printStats == 1)
                    incStatPixel(neighbour, texCoord[i]);
                //c += vec4(vec3(zn),1.0)*weight;
                //c = vec4(vec3(zn),1.0);
            }

sum /= 4.0f;
float disp = 0;
for(int i=0; i<4; i++)
//      disp += distance(colors[i],sum);
//      disp += distance(rgb2lab(colors[i]),rgb2lab(sum));
      disp += deltaE2000(colors[i],sum);
/*if(disp > 1.5)
c = vec4(1.0);*/
cols[j]=c;
cols[j].w = disp;
}
float d=cols[0].w;
c = vec4(cols[0].xyz,1.0);
for(int i=1;i<s;i++)
    if(cols[i].w < d)
    {
        d = cols[i].w;
        c = vec4(cols[i].xyz,1.0);
    }
/*if(d>9.5)
    c= vec4(1.0,0.0,0.0,1.0);*/

}
     else if(mode == 3)
        {

const float searchDelta = 0.05;
const int searchIterations = 10;
vec4 cols[searchIterations];
float znn = zn;
int s = (depth==1) ? 1 : searchIterations;
for(int j=0; j<s; j++)
{
zn=znn+(1.0-2*(j%2))*searchDelta*(j/2);
vec3 colors[64];
vec3 summ = vec3(0.0);

            float maxDistance = distance(vec2(0,0), vec2(gridSize.x-1, gridSize.y-1));
            float sum = 0;
            c = vec4(0);
            for(int x=0; x<gridSize.x; x++)
                for(int y=0; y<gridSize.y; y++)
                {      
                    float distance = maxDistance-distance(vec2(xSel, ySel), vec2(x,y));
                    sum += distance; 
                    //int slice = (gridIndex.y-y)*int(gridSize.x)+x;
                    int slice = y*int(gridSize.x)+x;
                    vec2 texCoord = intersCoord+(vec2(xSel,ySel)-vec2(x,y))*(zn/(1.0-zn));
                    texCoord += 0.5*scale;
                    texCoord /= scale;
                    float weight = distance;//1.0/64;
colors[slice] = (texture(lfTextures[slice], texCoord).xyz);
summ += (colors[slice].xyz); 
                    c += texture(lfTextures[slice],texCoord) * weight;
                }

                c /= sum;

summ /= 64.0f;
float disp = 0;
for(int i=0; i<64; i++)
//      disp += distance(colors[i],sum);
//      disp += distance(rgb2lab(colors[i]),rgb2lab(sum));
      disp += deltaE2000(colors[i],summ);
/*if(disp > 1.5)
c = vec4(1.0);*/
cols[j]=c;
cols[j].w = disp;
}
float d=cols[0].w;
c = vec4(cols[0].xyz,1.0);
for(int i=1;i<s;i++)
    if(cols[i].w < d)
    {
        d = cols[i].w;
        c = vec4(cols[i].xyz,1.0);
    }
        } 

        else
        {  
//30 0.04 -0.5
float v=999999;
vec4 color;
const int iterations = (depth==1) ? 30 : 1;
const float delta = 0.04;
const float start = -0.5;
float zt=0;
for(int i=0; i<iterations;i++)
{
c=vec4(0);
vec4 colorReal = vec4(0);
float zz = (depth==1) ? (start + i*delta) : z; 
            vec4 colors[64];
            float var=0;
            //TODO odd number of images
            for(int x=0; x<gridSize.x; x++)
                for(int y=0; y<gridSize.y; y++)
                { 
                    int slice = y*int(gridSize.x)+x;
                    ivec2 offset = ivec2(gridIndex.x-2*x, gridIndex.y-2*y);
                    offset.y *=-1;
                    vec2 focusedCoords = vCoord+offset*zz*0.01*vec2(1.0,aspect);
                    colors[slice] = blurSample(slice,3,focusedCoords);
                    c+=colors[slice];
                    colorReal += texture(lfTextures[slice],focusedCoords);
                }  
                c /= gridSize.x*gridSize.y;
                colorReal /= gridSize.x*gridSize.y;
                for(int i=0;i<64;i++)
                    var += deltaE2000/*distance*/(colors[i].xyz, c.xyz);
                var/=64;
if(var<v)
{v=var; color=colorReal;}
}
v/=70.0;
c=vec4(v,v,v,1.0);
c=color;
        }  
}
    fColor = c;
}
