#version 330
#include "common.glsl"

vs2fs vec2 vs_uv;
vs2fs vec3 vs_pos;

#ifdef _VERTEX
	layout(location = 0) in vec2  pos;
	
	uniform sampler2D heightmap;
	
	uniform vec2 offset;
	uniform vec2 size;
	uniform vec2 inv_max_size;
	
	void main () {
		vec2 pos_world = size * pos + offset;
		vec2 uv = pos_world * inv_max_size;
		
		float z = texture(heightmap, uv).r * 1000.0 - 84.0;
		vec3 pos3 = vec3(pos_world, z);
		
		gl_Position = view.world2clip * vec4(pos3, 1.0);
		vs_uv = uv;
		vs_pos = pos3;
	}
#endif
#ifdef _FRAGMENT
	uniform sampler2D terrain_diffuse;
	
	out vec4 frag_col;
	void main () {
		vec3 col = texture(terrain_diffuse, vs_uv).rgb;
		if (vs_pos.z <= 0.0) col = mix(col, vec3(0.05,0.11,1), 0.3);
		frag_col = vec4(col, 1.0);
	}
#endif
