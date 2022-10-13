#pragma once
#include "common.hpp"
#include "camera.hpp"
#include "engine/dbgdraw.hpp"

struct Game {
	SERIALIZE(Game, cam, dbg_cam, view_dbg_cam, sun_azim, sun_elev, day_t, day_speed)
	
	Flycam cam = Flycam(float3(0,0,10), 0, 5);
	Flycam dbg_cam;
	bool view_dbg_cam = false;

	Game () {
		cam.clip_far = 10000;
		dbg_cam.clip_far = 10000;
	}

	DebugDraw dbgdraw;

	float sun_azim = deg(30); // degrees from east, counter clockwise
	float sun_elev = deg(14);
	float day_t = 0.6f; // [0,1] -> [0,24] hours
	float day_speed = 1.0f / 60.0f;
	bool  day_pause = true;

	void imgui () {
		ZoneScoped;

		if (ImGui::Begin("Misc")) {
			
			if (imgui_Header("Game", true)) {

				if (ImGui::TreeNode("Time of Day")) {

					ImGui::SliderAngle("sun_azim", &sun_azim, 0, 360);
					ImGui::SliderAngle("sun_elev", &sun_elev, -90, 90);
					ImGui::SliderFloat("day_t", &day_t, 0,1);

					ImGui::SliderFloat("day_speed", &day_speed, 0, 0.25f, "%.3f", ImGuiSliderFlags_Logarithmic);
					ImGui::Checkbox("day_pause", &day_pause);
			
					ImGui::TreePop();
				}

				cam.imgui("cam");
				dbg_cam.imgui("dbg_cam");
				ImGui::Checkbox("view_dbg_cam", &view_dbg_cam);

				ImGui::PopID();
			}
		}
		ImGui::End();
	}

	View3D view;

	float3 sun_dir;

	void update (Window& window) {
		ZoneScoped;

		auto& I = window.input;

		if (!day_pause) day_t = wrap(day_t + day_speed * I.dt, 1.0f);
		sun_dir = rotate3_Z(sun_azim) * rotate3_X(sun_elev) * rotate3_Y(day_t * deg(360)) * float3(0,0,-1);

		dbgdraw.clear();

		if (I.buttons[KEY_P].went_down) {
			view_dbg_cam = !view_dbg_cam;
			if (view_dbg_cam) {
				dbg_cam.pos = cam.pos;
				dbg_cam.rot_aer = cam.rot_aer;
			}
		}
		if (I.buttons[KEY_Q].went_down && view_dbg_cam) {
			cam.pos = dbg_cam.pos;
		}

		Flycam& viewed_cam = view_dbg_cam ? dbg_cam : cam;

		view = viewed_cam.update(I, (float2)I.window_size);

		dbgdraw.axis_gizmo(view, I.window_size);

		if (view_dbg_cam)
			dbgdraw.cylinder(cam.pos, 6, 8, lrgba(1,0,1,1));

		//for (int y=0; y<64; ++y)
		//for (int x=0; x<64; ++x) {
		//	dbgdraw.quad(float3(x + 0.5f, y + 0.5f, 0), 1, lrgba(1,1,1,1));
		//}
	}
};
