#version 430
#include "common.glsl"

vs2fs vec4 vs_col;

#ifdef _VERTEX
	layout(location = 0) in vec3  pos;
	layout(location = 1) in vec4  col;
	
	void main () {
		gl_Position = view.world2clip * vec4(pos, 1.0);
		vs_col   = col;
	}
#endif
#ifdef _FRAGMENT
	out vec4 frag_col;
	void main () {
		frag_col = vs_col;
	}
#endif
