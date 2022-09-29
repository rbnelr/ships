#version 430
#include "common.glsl"

vs2fs vec2 vs_uv;
vs2fs vec3 vs_pos;
vs2fs vec3 vs_normal;
vs2fs vec3 vs_dbg;

#ifdef _VERTEX
	layout(location = 0) in vec2  attr_pos;
	
	uniform sampler2D heightmap;
	
	uniform vec2  offset;
	uniform float quad_size;
	uniform vec2  inv_max_size;
	
	uniform vec2  lod_bound0;
	uniform vec2  lod_bound1;
	
	float lod_base = 0.0;
	float lod_fac = 64.0;
	
	// TODO: could be done on cpu side by providing a mesh variant for
	// center, edge and corner lod transitions and having the cpu code select them
	// they would have to be rotated with a 2x2 matrix
	void fix_lod_seam (inout vec2 pos) {
		vs_dbg = vec3(0);
		
		vec2 dist = min(lod_bound1 - pos, pos - lod_bound0);
		vec2 t = clamp(1.0 - dist / quad_size, vec2(0), vec2(1));
		
		float tm = max(t.x, t.y);
		
		float lod_quad_size = quad_size * (1.0 + tm);
		
		if (tm > 0.0) {
			
			vec2 shift = fract(attr_pos * 0.5) * 2.0 * quad_size;
			
			if (tm == t.x) pos.y -= shift.y * t.x;
			else           pos.x -= shift.x * t.y;
			
			vs_dbg = vec3(t, 0.0);
		}
	}
	
	float sample_heightmap (vec2 pos) {
		vec2 uv = pos * inv_max_size;
		return texture(heightmap, uv).r * 1000.0 - 84.0;
	}
	
	void main () {
		vec3 pos = vec3(quad_size * attr_pos + offset, 0.0);
		
		fix_lod_seam(pos.xy);
		
		pos.z = sample_heightmap(pos.xy);
		
		{ // numerical derivative of heightmap for normals
			float eps = max(quad_size, 2.0);
			vec3 b = vec3(pos.x + eps, pos.y, 0.0);
			vec3 c = vec3(pos.x, pos.y + eps, 0.0);
			
			b.z = sample_heightmap(b.xy);
			c.z = sample_heightmap(c.xy);
			
			vs_normal = normalize(cross(b - pos, c - pos));
		}
		
		gl_Position = view.world2clip * vec4(pos, 1.0);
		vs_uv  = pos.xy * inv_max_size;
		vs_pos = pos;
	}
#endif
#ifdef _FRAGMENT
	uniform sampler2D terrain_diffuse;
	
	uniform vec3 sun_dir = normalize(vec3(4, 3, 6));
	uniform vec3 sun_light = vec3(0.8, 0.77, 0.6) * 2.0;
	uniform vec3 amb_light = vec3(0.6, 0.7, 1.0) * 0.2;
	
	out vec4 frag_col;
	void main () {
		vec3 col = texture(terrain_diffuse, vs_uv).rgb;
		if (vs_pos.z <= 0.0) col = mix(col, vec3(0.05,0.11,1), 0.3);
		
		{
			float d = max(dot(sun_dir, vs_normal), 0.0);
			vec3 light = sun_light * d + amb_light;
			
			col *= light;
		}
		
		frag_col = vec4(col, 1.0);
	}
#endif
