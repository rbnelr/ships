#pragma once
#include "common.hpp"
#include "game.hpp"
#include "engine/opengl.hpp"

namespace ogl {

struct Renderer {
	//SERIALIZE(Renderer)
	
	StateManager state;
	CommonUniforms common_ubo;
	
	LineRenderer dbg_lines;
	TriRenderer dbg_tris;

	struct glDebugDraw {
		bool wireframe          = false;
		bool wireframe_no_cull  = false;
		bool wireframe_no_blend = true;

		float line_width = 1;

		void imgui () {
			if (ImGui::Begin("Debug Draw")) {
				ImGui::Checkbox("wireframe", &wireframe);
				ImGui::SameLine();
				ImGui::Checkbox("no cull", &wireframe_no_cull);
				ImGui::SameLine();
				ImGui::Checkbox("no blend", &wireframe_no_blend);

				ImGui::SliderFloat("line_width", &line_width, 1.0f, 8.0f);
			}
			ImGui::End();
		}
	} debug_draw;

	void imgui () {
		debug_draw.imgui();
	}

	void render (Game& game, int2 window_size) {
		
		{
			//OGL_TRACE("set state defaults");

			state.wireframe          = debug_draw.wireframe;
			state.wireframe_no_cull  = debug_draw.wireframe_no_cull;
			state.wireframe_no_blend = debug_draw.wireframe_no_blend;

			state.set_default();

			glEnable(GL_LINE_SMOOTH);
			glLineWidth(debug_draw.line_width);
		}
		
		common_ubo.set(game.view);

		glViewport(0, 0, window_size.x, window_size.y);

		glClearColor(0.01f, 0.02f, 0.03f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		dbg_lines.render(state, game.dbgdraw.lines);
		dbg_tris.render(state, game.dbgdraw.tris);
	}
};

}