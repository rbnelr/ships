#version 430
#include "common.glsl"

vs2fs vec2 vs_uv;
vs2fs vec3 vs_pos;
vs2fs vec3 vs_normal;
vs2fs vec3 vs_dbg;

uniform float water_anim;

uniform vec2  offset;
uniform float quad_size;
uniform vec2  inv_max_size;

uniform vec2  lod_bound0;
uniform vec2  lod_bound1;

#ifdef _VERTEX
	layout(location = 0) in vec2  attr_pos;
	
	// TODO: could be done on cpu side by providing a mesh variant for
	// center, edge and corner lod transitions and having the cpu code select them
	// they would have to be rotated with a 2x2 matrix
	void fix_lod_seam (inout vec2 pos) {
		vs_dbg = vec3(0);
		
		vec2 dist = min(lod_bound1 - pos, pos - lod_bound0);
		vec2 t = clamp(1.0 - dist / quad_size, vec2(0), vec2(1));
		
		float tm = max(t.x, t.y);
		
		if (tm > 0.0) {
			
			vec2 shift = fract(attr_pos * 0.5) * 2.0 * quad_size;
			
			if (tm == t.x) pos.y -= shift.y * t.x;
			else           pos.x -= shift.x * t.y;
			
			vs_dbg = vec3(t, 0.0);
		}
	}
	
	vec3 gersner_wave (vec2 pos, float t, vec2 dir, float steep, float len) {
		float d = dot(dir, pos);
		
		float k = 2.0 * PI / len;
		float amp = steep / k;
		float c = sqrt(9.81 / k);
		float f = k * (d - c*t);
		
		float xy = cos(f) * amp;
		float z  = sin(f) * amp;
		
		return vec3(xy * dir, z);
	}
	
	vec3 wave_vertex (vec2 pos) {
		vec3 p = vec3(pos, 0.0);
		
		p += gersner_wave(pos, water_anim, normalize(vec2(7, -1)), 0.18, 9.0);
		p += gersner_wave(pos, water_anim, normalize(vec2(-7.3, 8)), 0.15, 8.7);
		p += gersner_wave(pos, water_anim, normalize(vec2(7.15, 1.2)), 0.14, 4.3);
		p += gersner_wave(pos, water_anim, normalize(vec2(17.1, 11)), 0.1, 1.3);
		p += gersner_wave(pos, water_anim, normalize(vec2(6.1, -13.1)), 0.1, 1.1);
		
		return p;
	}
	
	void main () {
		vec3 pos = vec3(quad_size * attr_pos + offset, 0.0);
		
		fix_lod_seam(pos.xy);
		
		vec3 a = wave_vertex(pos.xy);
		
		{ // numerical derivative of heightmap for normals
			float eps = max(quad_size, 2.0);
			vec3 b = vec3(pos.x + eps, pos.y, 0.0);
			vec3 c = vec3(pos.x, pos.y + eps, 0.0);
			
			b = wave_vertex(b.xy);
			c = wave_vertex(c.xy);
			
			vs_normal = normalize(cross(b - a, c - a));
		}
		
		gl_Position = view.world2clip * vec4(a, 1.0);
		vs_uv  = pos.xy * inv_max_size;
		vs_pos = a;
	}
#endif
#ifdef _FRAGMENT
	
	out vec4 frag_col;
	void main () {
		vec2 orig_pos = vs_uv / inv_max_size;
		
		vec3 col = vec3(0.12, 0.2, 1.0);
		col *= (fract(orig_pos.x)>0.5) == (fract(orig_pos.y)>0.5) ? 1.0 : 0.8;
		
		col *= simple_lighting(vs_normal);
		
		col = apply_fog(col, vs_pos);
		
		frag_col = vec4(col, 1.0);
	}
#endif
