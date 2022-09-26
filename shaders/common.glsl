
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

// layout(std140, binding = 0) only in #version 420
layout(std140) uniform Common {
	View3D view;
};
