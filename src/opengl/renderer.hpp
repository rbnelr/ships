#pragma once
#include "common.hpp"
#include "game.hpp"
#include "engine/opengl.hpp"

namespace ogl {

void push_quad (uint16_t* out, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
	out[0] = b;
	out[1] = c;
	out[2] = a;
	out[3] = a;
	out[4] = c;
	out[5] = d;
}

struct glDebugDraw {
	bool wireframe          = false;
	bool wireframe_no_cull  = false;
	bool wireframe_no_blend = true;

	float line_width = 1;

	void imgui () {
		if (imgui_Header("Debug Draw")) {
			ImGui::Checkbox("wireframe", &wireframe);
			ImGui::SameLine();
			ImGui::Checkbox("no cull", &wireframe_no_cull);
			ImGui::SameLine();
			ImGui::Checkbox("no blend", &wireframe_no_blend);

			ImGui::SliderFloat("line_width", &line_width, 1.0f, 8.0f);

			ImGui::PopID();
		}
	}
};

struct Renderer {
	//SERIALIZE(Renderer)
	
	StateManager state;
	CommonUniforms common_ubo;
	
	LineRenderer dbg_lines;
	TriRenderer dbg_tris;

	Sampler sampler_heightmap = {"sampler_heightmap"};
	Sampler sampler_normal    = {"sampler_normal"};

	glDebugDraw debug_draw;
	
	float lod_base = 0;
	float lod_fac  = 16.0f;
	
	struct TerrainRenderer {

		Shader* shad = g_shaders.compile("terrain");
		
		const char* heightmap_filename = "textures/Dark Alien Landscape/Height Map.hdr";
		const char* diffuse_filename   = "textures/Dark Alien Landscape/Diffuse Map.png";
		//const char* heightmap_filename = "textures/Great Lakes/Height Map.hdr";
		//const char* diffuse_filename   = "textures/Great Lakes/Diffuse Map.png";
		//const char* heightmap_filename = "textures/Rocky Land and Rivers/Height Map.hdr";
		//const char* diffuse_filename   = "textures/Rocky Land and Rivers/Diffuse.png";

		Texture2D heightmap = {"heightmap"};
		Texture2D terrain_diffuse = {"terrain_diffuse"};

		TerrainRenderer () {
			upload_heightmap();
			gen_terrain_quad();
		}

		void upload_heightmap () {
			{
				Image<float> img;
				if (!Image<float>::load_from_file(heightmap_filename, &img)) {
					assert(false);
					return;
				}

				glBindTexture(GL_TEXTURE_2D, heightmap);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, img.size.x, img.size.y, 0, GL_RED, GL_FLOAT, img.pixels);

				glGenerateMipmap(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
			{
				Image<srgb8> img;
				if (!Image<srgb8>::load_from_file(diffuse_filename, &img)) {
					assert(false);
					return;
				}

				glBindTexture(GL_TEXTURE_2D, terrain_diffuse);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, img.size.x, img.size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, img.pixels);

				glGenerateMipmap(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
		}
	
		struct TerrainVertex {
			float2 pos;

			static void attrib (int idx) {
				ATTRIB(idx++, GL_FLOAT,2, TerrainVertex, pos);
			}
		};
		VertexBufferI terrain_chunk = vertex_bufferI<TerrainVertex>("terrain");
		int terrain_indices;
		
		static constexpr int MAP_SZ = 2048;

		static constexpr int   TERRAIN_CHUNK_SZ = 64; // 128 is max with GL_UNSIGNED_SHORT indices
		static constexpr int   TERRAIN_LOD0_DIV = 4; // Divisor for smaller than 1m
		static constexpr float TERRAIN_LOD0_SZ = 1.0f / TERRAIN_LOD0_DIV; // 128 is max with GL_UNSIGNED_SHORT indices
		static constexpr int   TERRAIN_LODS = get_const_log2(2 * MAP_SZ / TERRAIN_CHUNK_SZ * TERRAIN_LOD0_DIV);

		void gen_terrain_quad () {
			
			constexpr int sz = TERRAIN_CHUNK_SZ;
			
			std::vector<TerrainVertex> verts( (sz+1) * (sz+1) );
			std::vector<uint16_t>      indices( sz * sz * 6 );

			int vert_count=0;
			for (int y=0; y<sz+1; ++y)
			for (int x=0; x<sz+1; ++x) {
				verts[vert_count++] = TerrainVertex{ float2(x / (float)sz, y / (float)sz) };
			}

			int idx_count = 0;
			for (int y=0; y<sz; ++y)
			for (int x=0; x<sz; ++x) {
				push_quad(&indices[idx_count],
					(uint16_t)( (y  )*(sz+1) + (x  ) ),
					(uint16_t)( (y  )*(sz+1) + (x+1) ),
					(uint16_t)( (y+1)*(sz+1) + (x+1) ),
					(uint16_t)( (y+1)*(sz+1) + (x  ) )
				);
				idx_count += 6;
			}

			terrain_chunk.upload(verts, indices);
			terrain_indices = idx_count;
		}
		void draw_terrain (Game& g, Renderer& r) {
			PipelineState s;
			// default
			r.state.set(s);

			glUseProgram(shad->prog);
			
			glBindTexture(GL_TEXTURE_2D, heightmap);

			r.state.bind_textures(shad, {
				{"heightmap", heightmap, r.sampler_heightmap},
				{"terrain_diffuse", terrain_diffuse, r.sampler_normal},
			});
			
			shad->set_uniform("inv_max_size", 1.0f / float2((float)MAP_SZ));

			glBindVertexArray(terrain_chunk.vao);
			
			float2 lod_center = (float2)g.cam.pos;
			// TODO: adjust for terrain height? -> abs distance to full heightmap range might be reasonable
			// -> finding correct "distance" to heightmap terrain is somewhat impossible
			//    there could always be cases where a chunk has too high of a LOD compared to the distance the surface
			//    actually ends up being to the camera due to the heightmap
			float lod_center_z = (float)g.cam.pos.z;

			int2 prev_start = 0;
			int2 prev_end = 0;

			// iterate lods
			for (int lod=0; lod<TERRAIN_LODS; lod++) {

				// lod radius formula
				float radius = r.lod_base + r.lod_fac * (float)(1 << lod);
				if (lod_center_z >= radius)
					continue;
				radius = sqrt(radius*radius - lod_center_z*lod_center_z);

				int sz        = (TERRAIN_CHUNK_SZ / TERRAIN_LOD0_DIV) << lod; // size of this lod grid
				int parent_sz = (TERRAIN_CHUNK_SZ / TERRAIN_LOD0_DIV) << (lod+1); // size of parent lod grid
				
				int parent_mask = ~(parent_sz-1);

				// for this lod radius, get the chunk grid bounds, aligned such that the parent lod chunks still fit without holes
				int2 start =  (int2)(lod_center - radius) & parent_mask;
				int2 end   = ((int2)(lod_center + radius) & parent_mask) + parent_sz;
				// clamp to heightmap bounds
				//start = max(start, int2(0));
				//end   = min(end,   int2(MAP_SZ));

				// interate over all chunks in bounds
				for (int y=start.y; y<end.y; y+=sz)
				for (int x=start.x; x<end.x; x+=sz) {
					// exclude chunks that are already covered by smaller LOD chunks
					if ( x >= prev_start.x && x < prev_end.x &&
					     y >= prev_start.y && y < prev_end.y )
						continue;
					
					g.dbgdraw.wire_quad(float3((float2)int2(x,y), 0), (float2)(float)sz, DebugDraw::COLS[lod % ARRLEN(DebugDraw::COLS)]);

					// draw chunk with this lod
					shad->set_uniform("offset", float2(int2(x,y)));
					shad->set_uniform("size",   float2((float)sz));
					
					glDrawElements(GL_TRIANGLES, terrain_indices, GL_UNSIGNED_SHORT, (void*)0);
				}

				prev_start = start;
				prev_end = end;
			}
		}
	};
	TerrainRenderer terrain_renderer;

	Renderer () {
		float max_aniso = 1.0f;
		glGetFloatv(GL_TEXTURE_MAX_ANISOTROPY, &max_aniso);

		{
			glSamplerParameteri(sampler_heightmap, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glSamplerParameteri(sampler_heightmap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glSamplerParameteri(sampler_heightmap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler_heightmap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			
			glSamplerParameterf(sampler_heightmap, GL_TEXTURE_MAX_ANISOTROPY, 1.0f);
		}
		{
			glSamplerParameteri(sampler_normal, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glSamplerParameteri(sampler_normal, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glSamplerParameteri(sampler_normal, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler_normal, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			
			glSamplerParameterf(sampler_normal, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);
		}
	}

	void imgui (Input& I) {
		if (ImGui::Begin("Misc")) {
			debug_draw.imgui();

			ImGui::DragFloat("lod_base", &lod_base, 1, 0, terrain_renderer.MAP_SZ);
			ImGui::DragFloat("lod_fac", &lod_fac, 1, 0, terrain_renderer.MAP_SZ);
		}
		ImGui::End();
	}

	void render (Window& window, Game& g, int2 window_size) {
		
		{
			//OGL_TRACE("set state defaults");

			state.wireframe          = debug_draw.wireframe;
			state.wireframe_no_cull  = debug_draw.wireframe_no_cull;
			state.wireframe_no_blend = debug_draw.wireframe_no_blend;

			state.set_default();

			glEnable(GL_LINE_SMOOTH);
			glLineWidth(debug_draw.line_width);
		}
		
		common_ubo.set(g.view);

		glViewport(0, 0, window_size.x, window_size.y);

		glClearColor(0.01f, 0.02f, 0.03f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		terrain_renderer.draw_terrain(g, *this);

		dbg_lines.render(state, g.dbgdraw.lines);
		dbg_tris.render(state, g.dbgdraw.tris);
		
		if (window.trigger_screenshot && !window.screenshot_hud) take_screenshot(window_size);
		
		// draw HUD

		if (window.trigger_screenshot && window.screenshot_hud)  take_screenshot(window_size);
		window.trigger_screenshot = false;
	}
};

}