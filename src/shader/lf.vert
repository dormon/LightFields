uniform mat4 mvp;
uniform float aspect = 1.f;
out vec2 vCoord;

void main()
{

    vCoord = vec2(gl_VertexID&1,gl_VertexID>>1);
    gl_Position = mvp*vec4((-1+2*vCoord)*vec2(aspect,1),0,1);
    /*
    	if(gl_VertexID == 0)
    		gl_Position = vec4(0.0, 1.0, 0.0, 1.0);
    	else if(gl_VertexID == 1)
    		gl_Position = vec4(1.0, 1.0, 0.0, 1.0);
    	else if(gl_VertexID == 2)
    		gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    	else
    		gl_Position = vec4(1.0, 0.0, 0.0, 1.0);

    	gl_Position = mvp*gl_Position;*/
}
