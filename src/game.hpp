#pragma once
#include "common.hpp"
#include "camera.hpp"
#include "engine/dbgdraw.hpp"

struct Game {
	SERIALIZE(Game, cam)
	
	Flycam cam;

	DebugDraw dbgdraw;

	float sun_azim = deg(30); // degrees from east, counter clockwise
	float sun_elev = deg(70);
	float sun_t = 0.75; // [0,1] -> [0,24] hours

	void imgui () {
		if (ImGui::Begin("Time of Day")) {

			ImGui::SliderAngle("sun_azim", &sun_azim, 0, 360);
			ImGui::SliderAngle("sun_elev", &sun_elev, -90, 90);
			ImGui::SliderFloat("sun_t", &sun_t, 0,1);

		}
		ImGui::End();

		if (ImGui::Begin("Game")) {
			cam.imgui();
		}
		ImGui::End();
	}

	View3D view;

	void update (Window& window) {
		auto& I = window.input;

		dbgdraw.clear();

		view = cam.update(I, (float2)I.window_size);
		dbgdraw.axis_gizmo(view, I.window_size);

		for (int y=0; y<64; ++y)
		for (int x=0; x<64; ++x) {
			dbgdraw.quad(float3(x + 0.5f, y + 0.5f, 0), 1, lrgba(1,1,1,1));
		}
	}
};
