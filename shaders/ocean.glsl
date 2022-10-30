#version 430

#pragma VISUALIZE_NORMALS
#include "common.glsl"

struct Vertex {
	vec3 pos;
	vec2 fbo_uv;
	vec2 uv;
	vec3 normal;
};
VS2FS

uniform float water_anim;

uniform vec2  offset;
uniform float quad_size;
uniform vec2  inv_max_size;

uniform vec2  lod_bound0;
uniform vec2  lod_bound1;

uniform float water_roughness;

// xy = dir  z = steepness  w = wave length
uniform vec4 waves[8+1] = {
	vec4(normalize(vec2(4.7, -3.3)), 0.20,  97.0),
	vec4(normalize(vec2(7, -1)),     0.18,  17.0),
	vec4(normalize(vec2(-7.3, 8)),   0.15,  13.0),
	vec4(normalize(vec2(3.15, 1.2)), 0.14,   7.0),
	vec4(normalize(vec2(-17.1, 11)), 0.10,   3.0),
	vec4(0),
	vec4(0),
	vec4(0),
	vec4(0),
};

#ifdef _VERTEX
	layout(location = 0) in vec2  attr_pos;
	
	// TODO: could be done on cpu side by providing a mesh variant for
	// center, edge and corner lod transitions and having the cpu code select them
	// they would have to be rotated with a 2x2 matrix
	void fix_lod_seam (inout vec2 pos) {
		vec2 dist = min(lod_bound1 - pos, pos - lod_bound0);
		vec2 t = clamp(1.0 - dist / quad_size, vec2(0), vec2(1));
		
		float tm = max(t.x, t.y);
		
		if (tm > 0.0) {
			
			vec2 shift = fract(attr_pos * 0.5) * 2.0 * quad_size;
			
			if (tm == t.x) pos.y -= shift.y * t.x;
			else           pos.x -= shift.x * t.y;
		}
	}
	
	vec3 gersner_wave (vec2 pos, inout vec3 tang, inout vec3 bitang,
			float t, float steep, vec2 dir, float len) {
		//t = 0.0;
		
		float d = dot(dir, pos);
		
		float d2 = dot(vec2(-dir.y, dir.x), pos);
		d += sin(d2 / len * 1.2) * len * 0.25;
		
		float k = 2.0 * PI / len;
		float amp = steep / k;
		float c = sqrt(9.81 / k);
		float f = k * (d - c*t);
		
		float xy = cos(f) * amp;
		float z  = sin(f) * amp;
		
		vec3 p = vec3(xy * dir, z);
		
		
		float a = steep * sin(f);
		float b = steep * cos(f);
		
		tang   += vec3(-dir.x * dir.x * a,
		               -dir.x * dir.y * a,
		                dir.x * b);
		bitang += vec3(-dir.x * dir.y * a,
		               -dir.y * dir.y * a,
		                dir.y * b);
		return p;
	}
	
	vec3 wave_vertex (vec2 pos) {
		vec3 p = vec3(pos, 0.0);
		vec3 tang = vec3(1.0, 0.0, 0.0);
		vec3 bitang = vec3(0.0, 1.0, 0.0);
		
		//p += gersner_wave(pos, tang,bitang, water_anim, normalize(vec2(7, 3)), water_roughness, 9.0);
		//p += gersner_wave(pos, norm, water_anim, normalize(vec2(7, 0)), water_roughness, 9.0);
		
		for (int i=0; i<8 && waves[i].z > 0.0; ++i) {
			p += gersner_wave(pos, tang,bitang, water_anim,
				waves[i].z * water_roughness, waves[i].xy, waves[i].w);
		}
		
		v.normal = normalize(cross(tang, bitang));
		
		//normal = norm;
		return p;
	}
	
	void main () {
		vec3 pos = vec3(quad_size * attr_pos + offset, 0.0);
		
		fix_lod_seam(pos.xy);
		
		vec3 p = wave_vertex(pos.xy);
		
		if (_dbgdrawbuf.update &&
			abs(pos.x) < 10.0 && abs(pos.y) < 10.0) {
			dbgdraw_vector(p, v.normal, vec4(0,1,0,1));
		}
		
		gl_Position = view.world2clip * vec4(p, 1.0);
		v.pos    = p;
		v.fbo_uv = (gl_Position.xy / gl_Position.w) * 0.5 + 0.5;
		v.uv     = pos.xy * inv_max_size;
	}
#endif
#ifdef _FRAGMENT
	
	uniform sampler2D opaque_fbo;

	vec4 water_lighting (vec4 col, vec3 pos, vec3 normal) {
		vec3 dir = normalize(pos - view.cam_pos);
		
		//float d = max(dot(lighting.sun_dir, normal), 0.0);
		//vec3 diffuse = lighting.sun_col*2.0 * d + lighting.sky_col*0.3;
		
		vec3 refl_dir = reflect(dir, normal);
		vec3 specular = get_skybox_light(pos, refl_dir);
		
		float F = fresnel_roughness(dot(normal, -dir), 0.05, 0.1);
		
		//col.rgb *= 0.01;
		//col.a = 0.8;
		
		const float IOR_air   = 1.0;
		const float IOR_water = 1.3333;
		const float eta = IOR_air / IOR_water;
		
		vec3 refr_dir = refract(dir, normal, eta);
		vec3 refr_cam = mat3(view.world2cam) * refr_dir;
		
		//vec2 uv = v.fbo_uv + refr_cam.xy * 0.1;
		vec2 uv = v.fbo_uv;
		vec3 opaque_tex = texture(opaque_fbo, uv).rgb;
		
		col.rgb = opaque_tex * col.rgb;
		
		col.a = 1.0;
		col.rgb *= 0.05;
		
		//return col;
		return vec4(mix(col, vec4(specular, 1.0), F));
	}

	out vec4 frag_col;
	void main () {
		vec3 norm = normalize(v.normal);
		
		vec4 col = vec4(1.0, 1.0, 1.0, 1.0);
		
		//vec2 orig_pos = v.uv / inv_max_size;
		//col.rgb *= (fract(orig_pos.x)>0.5) == (fract(orig_pos.y)>0.5) ? 1.0 : 0.8;
		
		col = water_lighting(col, v.pos, norm);
		col.rgb = apply_fog(col.rgb, v.pos);
		
		frag_col = col;
	}
#endif
