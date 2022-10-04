
#ifdef _VERTEX
#define vs2fs out
#endif
#ifdef _FRAGMENT
#define vs2fs in
#endif

float map (float x, float a, float b) {
	return (x - a) / (b - a);
}

const float INF			= 1. / 0.;

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

vec3 get_skybox_light (vec3 dir_world) {
	vec3 col = vec3(0.0);
	//if (dir_world.z > 0.0) {
	//	col += mix(lighting.fog_col, lighting.sky_col,
	//		vec3(pow(smoothstep(0.0, 1.0, dir_world.z), 0.45)));
	//} else {
	//	col += mix(lighting.fog_col, lighting.skybox_bottom_col,
	//		vec3(pow(smoothstep(0.0, 1.0, -dir_world.z), 0.5)));
	//}
	col = lighting.sky_col;
	
	// sun
	float d = dot(dir_world, lighting.sun_dir);
	
	const float sz = 500.0;
	float c = clamp(d * sz - (sz-1.0), 0.0, 1.0);
	
	col += lighting.sun_col * 2.0 * c;
	
	return col;
}

vec3 simple_lighting (vec3 normal) {
	
	float d = max(dot(lighting.sun_dir, normal), 0.0);
	vec3 light = lighting.sun_col*2.0 * d + lighting.sky_col*0.3;
	
	return vec3(light);
}
vec3 apply_fog (vec3 pix_col, vec3 pix_pos) {
	// from https://iquilezles.org/articles/fog/
	// + my own research and derivation
	
	// TODO: properly compute extinction (out-scattering + absoption) and in-scattering
	// seperating in-scattering may allow for more blue tint at distance
	
	vec3 ray_cam = pix_pos - view.cam_pos;
	float dist = length(ray_cam);
	ray_cam = normalize(ray_cam);
	
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
	vec3  col = mix(lighting.fog_col, lighting.sun_col, pow(sun_amount, 8.0)*0.5);
	
	//return vec3(1.0 - t);
	
	// lerp pixel color to fog color
	return mix(col, pix_col, t);
}
