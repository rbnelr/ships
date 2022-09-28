#pragma once
#include "common.hpp"
#include "camera.hpp"
#include "engine/dbgdraw.hpp"

struct Game {
	SERIALIZE(Game, cam, dbg_cam, view_dbg_cam)
	
	Flycam cam;
	Flycam dbg_cam;
	bool view_dbg_cam = false;

	DebugDraw dbgdraw;

	float sun_azim = deg(30); // degrees from east, counter clockwise
	float sun_elev = deg(70);
	float sun_t = 0.75; // [0,1] -> [0,24] hours

	void imgui () {
		if (ImGui::Begin("Misc")) {
			
			if (imgui_Header("Game", true)) {

				if (ImGui::TreeNode("Time of Day")) {

					ImGui::SliderAngle("sun_azim", &sun_azim, 0, 360);
					ImGui::SliderAngle("sun_elev", &sun_elev, -90, 90);
					ImGui::SliderFloat("sun_t", &sun_t, 0,1);
			
					ImGui::TreePop();
				}

				cam.imgui();
				ImGui::PopID();
			}
		}
		ImGui::End();
	}

	View3D view;

	void update (Window& window) {
		auto& I = window.input;

		dbgdraw.clear();

		if (I.buttons[KEY_P].went_down) {
			view_dbg_cam = !view_dbg_cam;
			if (view_dbg_cam) {
				dbg_cam.pos = cam.pos;
				dbg_cam.rot_aer = cam.rot_aer;
			}
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
