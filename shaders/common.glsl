
#if defined(_VERTEX)
	#define VS2FS out Vertex v;
#elif defined(_FRAGMENT)
	#define VS2FS in Vertex v;
#endif

float map (float x, float a, float b) {
	return (x - a) / (b - a);
}

const float INF			= 340282346638528859811704183484516925440.0 * 2.0;
//const float INF			= 1. / 0.;

const float PI			= 3.1415926535897932384626433832795;

const float DEG2RAD		= 0.01745329251994329576923690768489;
const float RAD2DEG		= 57.295779513082320876798154814105;

const float SQRT_2	    = 1.4142135623730950488016887242097;
const float SQRT_3	    = 1.7320508075688772935274463415059;

const float HALF_SQRT_2	= 0.70710678118654752440084436210485;
const float HALF_SQRT_3	= 0.86602540378443864676372317075294;

const float INV_SQRT_2	= 0.70710678118654752440084436210485;
const float INV_SQRT_3	= 0.5773502691896257645091487805019;

struct View3D {
	// forward VP matrices
	mat4        world2clip;
	mat4        world2cam;
	mat4        cam2clip;

	// inverse VP matrices
	mat4        clip2world;
	mat4        cam2world;
	mat4        clip2cam;

	// more details for simpler calculations
	vec2        frust_near_size; // width and height of near plane (for casting rays for example)
	// near & far planes
	float       clip_near;
	float       clip_far;
	// just the camera center in world space
	vec3        cam_pos;
	// viewport width over height, eg. 16/9
	float       aspect_ratio;
	
	// viewport size (pixels)
	vec2        viewport_size;
	vec2        inv_viewport_size;
};

struct Lighting {
	vec3 sun_dir;
	
	vec3 sun_col;
	vec3 sky_col;
	vec3 skybox_bottom_col;
	vec3 fog_col;
	
	float fog_base;
	float fog_falloff;
};

// layout(std140, binding = 0) only in #version 420
layout(std140, binding = 0) uniform Common {
	View3D view;
	Lighting lighting;
};

#include "dbg_indirect_draw.glsl"

uniform sampler2D clouds;

float sun_strength () {
	float a = map(lighting.sun_dir.z, 0.5, -0.1);
	return smoothstep(1.0, 0.0, a);
}
vec3 atmos_scattering () {
	float a = map(lighting.sun_dir.z, 0.5, -0.05);
	return vec3(0.0, 0.75, 0.8) * smoothstep(0.0, 1.0, a);
}

vec3 get_skybox_light (vec3 view_point, vec3 dir_world) {
	float stren = sun_strength();
	
	vec3 col = lighting.sky_col * (stren + 0.008);
	
	vec3 sun = lighting.sun_col - atmos_scattering();
	sun *= stren;
	
	// sun
	float d = dot(dir_world, lighting.sun_dir);
	
	const float sz = 500.0;
	float c = clamp(d * sz - (sz-1.0), 0.0, 1.0);
	
	col += sun * 20.0 * c;
	
	{
		float clouds_z = 200.0;
		
		float t = (clouds_z - view_point.z) / dir_world.z;
		if (dir_world.z != 0.0 && t >= 0.0) {
			vec3 pos = view_point + t * dir_world;
			
			pos /= 1024.0;
			vec4 c = texture(clouds, pos.xy);
			c.rgb *= stren;
			
			col = mix(col.rgb, c.rgb, vec3(c.a));
		}
	}
	
	float bloom_amount = max(dot(dir_world, lighting.sun_dir) - 0.5, 0.0);
	col += bloom_amount * sun * 0.3;
	
	return col;
} 

vec3 apply_fog (vec3 pix_col, vec3 pix_pos) {
	// from https://iquilezles.org/articles/fog/
	// + my own research and derivation
	
	// TODO: properly compute extinction (out-scattering + absoption) and in-scattering
	// seperating in-scattering may allow for more blue tint at distance
	
	vec3 ray_cam = pix_pos - view.cam_pos;
	float dist = length(ray_cam);
	ray_cam = normalize(ray_cam);
	
	float stren = sun_strength();
	
	// exponential height fog parameters
	float a = lighting.fog_base;
	float b = lighting.fog_falloff;
	// view parameters
	float c = view.cam_pos.z;
	float d = ray_cam.z;
	
	// optical depth -> total "amount" of fog encountered
	// horrible float precision at high camera offsets:
	//  1.0 - exp(-b*d*10000)  ->  1.0 - 10^-44 -> 1.0, which results in black screen
	//float od = (a/b) * exp(-b * c) * (1.0 - exp(-b * d * dist)) / d;
	// this seems to fix it  TODO: better alternative?
	float od = (a/b) * (exp(-b * c) - exp(-b * c - b * d * dist)) / d;
	
	// get transmittance (% of rays scattered/absorbed) from optical depth
	// -> missing in iquilezles's code?
	float t = exp(-od);
	
	// adjust color to give sun tint like iquilezles
	float sun_amount = max(dot(ray_cam, lighting.sun_dir), 0.0);
	//vec3  col = mix(lighting.fog_col, lighting.sun_col, pow(sun_amount, 8.0) * 0.5);
	vec3 sun = lighting.sun_col - atmos_scattering();
	
	vec3 col = mix(lighting.fog_col, sun, pow(sun_amount, 8.0) * 0.7);
	
	//return vec3(1.0 - t);
	
	// lerp pixel color to fog color
	return mix(col * stren, pix_col, t);
}

float fresnel (float dotVN, float F0) {
	float x = clamp(1.0 - dotVN, 0.0, 1.0);
	float x2 = x*x;
	return F0 + ((1.0 - F0) * x2 * x2 * x);
}
float fresnel_roughness (float dotVN, float F0, float roughness) {
	float x = clamp(1.0 - dotVN, 0.0, 1.0);
	float x2 = x*x;
	return F0 + ((max((1.0 - roughness), F0) - F0) * x2 * x2 * x);
}

vec3 simple_lighting (vec3 pos, vec3 normal) {
	float stren = sun_strength();
	
	vec3 sun = lighting.sun_col - atmos_scattering();
	
	//vec3 dir = normalize(pos - view.cam_pos);
	
	float d = max(dot(lighting.sun_dir, normal), 0.0);
	vec3 diffuse =
		(stren        ) * sun * d +
		(stren + 0.008) * lighting.sky_col*0.3;
	
	return vec3(diffuse);
}
