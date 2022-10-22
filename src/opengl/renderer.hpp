#pragma once
#include "common.hpp"
#include "game.hpp"
#include "engine/opengl.hpp"
#include "gl_dbgdraw.hpp"

namespace ogl {

void push_quad (uint16_t* out, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
	// D---C
	// | / |
	// A---B
	out[0] = b;
	out[1] = c;
	out[2] = a;
	out[3] = a;
	out[4] = c;
	out[5] = d;
}
void push_quad2 (uint16_t* out, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
	// D---C
	// | \ |
	// A---B
	out[0] = a;
	out[1] = b;
	out[2] = d;
	out[3] = d;
	out[4] = b;
	out[5] = c;
}

struct Renderer {
	SERIALIZE(Renderer, lighting, fbo.renderscale, exposure)
	
	void imgui (Input& I) {
		if (ImGui::Begin("Misc")) {
			if (imgui_Header("Renderer", true)) {

				fbo.imgui();

				ImGui::Checkbox("reverse_depth", &ogl::reverse_depth);

				debug_draw.imgui();
				lighting.imgui();
				ocean.imgui();
				terrain_renderer.imgui();

				ImGui::SliderFloat("exposure", &exposure, 0.02f, 20.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

				ImGui::PopID();
			}
		}
		ImGui::End();
	}

	StateManager state;
	
	Vao dummy_vao = {"dummy_vao"};

	void draw_fullscreen_triangle () {
		PipelineState s;
		s.depth_write  = false;
		s.blend_enable = false;
		state.set_no_override(s);
				
		glBindVertexArray(dummy_vao);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	struct Lighting {
		SERIALIZE(Lighting, sun_col, sky_col, fog_col, fog_base, fog_falloff)

		float4 sun_dir;

		lrgb sun_col = lrgb(0.8f, 0.77f, 0.4f) * 2.0f;
		float _pad0;

		lrgb sky_col = srgb(210, 230, 255);
		float _pad1;
		
		lrgb skybox_bottom_col = srgb(40,50,60);
		float _pad2;
		
		lrgb fog_col = srgb(210, 230, 255);
		//float _pad3;

		float fog_base = 2.00f;
		float fog_falloff = 10.0f;

		void imgui () {
			if (imgui_Header("Lighting", true)) {

				imgui_ColorEdit("sun_col", &sun_col);
				imgui_ColorEdit("sky_col", &sky_col);
				imgui_ColorEdit("fog_col", &fog_col);

				ImGui::DragFloat("fog_base/100", &fog_base, 0.001f);
				ImGui::DragFloat("fog_falloff/100", &fog_falloff, 0.01f);

				ImGui::PopID();
			}
		}
	};

	struct CommonUniforms {
		static constexpr int UBO_BINDING = 0;

		Ubo ubo = {"common_ubo"};

		struct Common {
			View3D view;
			Lighting lighting;
		};

		void set (View3D const& view, Lighting& l) {
			Common common = {};
			common.view = view;
			common.lighting = l;
			common.lighting.fog_base = l.fog_base / 100;
			common.lighting.fog_falloff = l.fog_falloff / 100;
			stream_buffer(GL_UNIFORM_BUFFER, ubo, sizeof(common), &common, GL_STREAM_DRAW);

			glBindBuffer(GL_UNIFORM_BUFFER, ubo);
			glBindBufferBase(GL_UNIFORM_BUFFER, UBO_BINDING, ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}
	};
	CommonUniforms common_ubo;

	FramebufferTexture fbo;

	Sampler sampler_heightmap  = sampler("sampler_heightmap", FILTER_BILINEAR,  GL_REPEAT);
	Sampler sampler_normal     = sampler("sampler_normal",    FILTER_MIPMAPPED, GL_REPEAT, true);

	glDebugDraw debug_draw;

	Lighting lighting;

	Texture2D clouds = {"clouds"};

	float exposure = 1.0f;
	
	Renderer () {
		if (!upload_texture2D<srgba8>(clouds, "textures/clouds.png")) assert(false);
	}
	
	struct SkyboxRenderer {
	
		Shader* shad = g_shaders.compile("skybox");

		struct Vertex {
			float3 pos;
		
			ATTRIBUTES {
				ATTRIB(idx++, GL_FLOAT,3, Vertex, pos);
			}
		};
		VertexBufferI skybox = vertex_bufferI<Vertex>("skybox");

		static constexpr float3 verts[] = {
			{-1,-1,-1},
			{+1,-1,-1},
			{-1,+1,-1},
			{+1,+1,-1},
			{-1,-1,+1},
			{+1,-1,+1},
			{-1,+1,+1},
			{+1,+1,+1},
		};
		static constexpr uint16_t indices[] = {
			// -Z 
			QUAD_INDICES(0,1,3,2),
			// +X
			QUAD_INDICES(3,1,5,7),
			// +Y
			QUAD_INDICES(2,3,7,6),
			// -X
			QUAD_INDICES(0,2,6,4),
			// -Y
			QUAD_INDICES(1,0,4,5),
			// +Z
			QUAD_INDICES(6,7,5,4),
		};

		SkyboxRenderer () {
			skybox.upload(verts, ARRLEN(verts), indices, ARRLEN(indices));
		}
	
	#if 0
		// draw before everything else
		// shader can use vec4(xyz, 1.0) clip coords
		void draw_skybox_first (StateManager& state) {
			if (!shad->prog) return;
			
			OGL_TRACE("draw_skybox");

			PipelineState s;
			s.depth_test = false;
			s.depth_write = false;
			s.blend_enable = false;
			state.set(s);

			glUseProgram(shad->prog);

			state.bind_textures(shad, {
			
			});
		
			glBindVertexArray(skybox.vao);

			glDrawElements(GL_TRIANGLES, ARRLEN(indices), GL_UNSIGNED_SHORT, (void*)0);
		}
	#else
		// draw after everything else
		// advantage: early-z avoids drawing skybox shader behind ground, might be better with scattering skybox shader
		// shader needs to use vec4(xyz, 0.0) clip coords to draw at infinity -> need to clip at far plane with GL_DEPTH_CLAMP
		void draw_skybox_last (StateManager& state, Renderer& r) {
			if (!shad->prog) return;
		
			OGL_TRACE("draw_skybox");

			PipelineState s;
			s.depth_test   = true;
			s.depth_write  = false;
			s.blend_enable = false;
			state.set_no_override(s);

			glEnable(GL_DEPTH_CLAMP);

			glUseProgram(shad->prog);

			state.bind_textures(shad, {
				{ "clouds", r.clouds, r.sampler_normal }
			});
		
			glBindVertexArray(skybox.vao);

			glDrawElements(GL_TRIANGLES, ARRLEN(indices), GL_UNSIGNED_SHORT, (void*)0);

			glDisable(GL_DEPTH_CLAMP);
		}
	#endif
	};
	SkyboxRenderer skybox;

	struct Ocean {

		float water_anim = 0;
		float water_roughness = 0.5f;

		// xy = dir  z = steepness  w = wave length
		float4 waves[8+1] {

			float4(normalize(float2(  4.70f, -3.3f)), 0.20f,  97.0f),
			float4(normalize(float2(  7.00f, -1.0f)), 0.18f,  17.0f),
			float4(normalize(float2(- 7.30f,  8.0f)), 0.15f,  13.0f),
			float4(normalize(float2(  3.15f,  1.2f)), 0.14f,   7.0f),
			float4(normalize(float2(-17.10f, 11.0f)), 0.10f,   3.0f),
			// zero terminated using z
		};

		void imgui () {
			if (imgui_Header("Ocean", true)) {

				ImGui::SliderFloat("water_roughness", &water_roughness, 0, 1);

				if (ImGui::BeginTable("waves", 3)) {

					ImGui::TableSetupColumn("dir");
					ImGui::TableSetupColumn("steep");
					ImGui::TableSetupColumn("len");

					for (int i=0; i<8; ++i) {
						ImGui::TableNextRow();
						ImGui::TableNextColumn();

						auto& w = waves[i];

						float ang = atan2(w.x, w.y);
						if (ImGui::Button("Randomize"))
							ang = random.uniformf(0, deg(360));
						ImGui::SliderAngle("dir", &ang, 0, 360);
						w.x = cos(ang); w.y = sin(ang);

						ImGui::TableNextColumn();
						ImGui::TableNextColumn();
					}
					ImGui::EndTable();
				}

				ImGui::PopID();
			}
		}
		void update (Input& I) {
			water_anim += I.dt;
			water_anim = fmodf(water_anim, 32.0f);
		}
		void set_uniforms (Shader* shad) {
			if (imgui_Header("TerrainRenderer", true)) {

				shad->set_uniform("water_anim", water_anim);
				shad->set_uniform("water_roughness", water_roughness);

				shad->set_uniform_array("waves", waves, ARRLEN(waves));

				ImGui::PopID();
			}
		}
	};
	Ocean ocean;

	struct TerrainRenderer {
		
		static constexpr int MAP_SZ = 2048;
		
		static constexpr int   TERRAIN_CHUNK_SZ = 32; // 128 is max with GL_UNSIGNED_SHORT indices

		Shader* shad_terrain = g_shaders.compile("terrain");
		Shader* shad_water   = g_shaders.compile("ocean");
		
		const char* heightmap_filename = "textures/Dark Alien Landscape/Height Map.hdr";
		const char* diffuse_filename   = "textures/Dark Alien Landscape/Diffuse Map.png";
		//const char* heightmap_filename = "textures/Great Lakes/Height Map.hdr";
		//const char* diffuse_filename   = "textures/Great Lakes/Diffuse Map.png";
		//const char* heightmap_filename = "textures/Rocky Land and Rivers/Height Map.hdr";
		//const char* diffuse_filename   = "textures/Rocky Land and Rivers/Diffuse.png";

		Texture2D heightmap = {"heightmap"};
		Texture2D terrain_diffuse = {"terrain_diffuse"};
		
		bool draw_terrain = true;
		bool draw_water = true;

		bool dbg_lod = false;
		
		float lod_offset = 16;
		float lod_fac    = 64;

		int water_base_lod = -1;

		void imgui () {
			if (imgui_Header("TerrainRenderer", true)) {

				ImGui::Checkbox("dbg_lod", &dbg_lod);

				ImGui::Checkbox("draw_terrain", &draw_terrain);
				ImGui::Checkbox("draw_water", &draw_water);

				ImGui::DragFloat("lod_offset", &lod_offset, 1, 0, MAP_SZ);
				ImGui::DragFloat("lod_fac", &lod_fac, 1, 0, MAP_SZ);

				ImGui::SliderInt("water_base_lod", &water_base_lod, -6, 6);

				ImGui::Text("drawn_chunks: %4d  (%7d vertices)", drawn_chunks, drawn_chunks * chunk_vertices);

				ImGui::PopID();
			}
		}

		TerrainRenderer () {
			upload_heightmap();
			gen_terrain_quad();
		}

		void upload_heightmap () {
			if (!upload_texture2D<float>(heightmap, heightmap_filename, false)) assert(false);
			if (!upload_texture2D<srgb8>(terrain_diffuse, diffuse_filename, true)) assert(false);
		}
	
		struct TerrainVertex {
			float2 pos;

			ATTRIBUTES {
				ATTRIB(idx++, GL_FLOAT,2, TerrainVertex, pos);
			}
		};
		VertexBufferI terrain_chunk = vertex_bufferI<TerrainVertex>("terrain");
		int chunk_vertices;
		int chunk_indices;
		
		int drawn_chunks = 0;

		template <typename FUNC>
		void lodded_chunks (Game& g, Shader* shad, int base_lod, bool dbg, FUNC chunk) {
			ZoneScoped;

			float2 lod_center = (float2)g.cam.pos;
			// TODO: adjust for terrain height? -> abs distance to full heightmap range might be reasonable
			// -> finding correct "distance" to heightmap terrain is somewhat impossible
			//    there could always be cases where a chunk has too high of a LOD compared to the distance the surface
			//    actually ends up being to the camera due to the heightmap
			float lod_center_z = g.cam.pos.z;

			int2 prev_bound0 = 0;
			int2 prev_bound1 = 0;

			// iterate lods
			for (int lod=base_lod;; lod++) {
				ZoneScopedN("lod");

				int sz = lod >= 0 ? TERRAIN_CHUNK_SZ << lod : TERRAIN_CHUNK_SZ >> (-lod);
				if (sz > MAP_SZ*2)
					break;

				int parent_sz = sz << 1;
				int parent_mask = ~(parent_sz-1);

				float quad_size = (float)sz / (float)TERRAIN_CHUNK_SZ;

				// lod radius formula
				float radius = lod_offset + lod_fac * quad_size;
				if (lod_center_z >= radius)
					continue;
				radius = sqrt(radius*radius - lod_center_z*lod_center_z);

				// for this lod radius, get the chunk grid bounds, aligned such that the parent lod chunks still fit without holes
				int2 bound0 =  (int2)(lod_center - radius) & parent_mask;
				int2 bound1 = ((int2)(lod_center + radius) & parent_mask) + parent_sz;
				
				assert(bound1.x > bound0.x && bound1.y > bound0.y); // needed for next check to work (on prev_bound)

				// make sure there is always one chunk of buffer between any lod, ie. lod2 does not border lod4 directly
				// this is usually the case anyway, but avoid edge cases
				// > needed for lod seam fix to work
				if (prev_bound0.x != prev_bound1.x) { // if prev_bound is actually valid
					bound0 = min(bound0, (prev_bound0 & parent_mask) - parent_sz);
					bound1 = max(bound1, (prev_bound1 & parent_mask) + parent_sz);
				}

				//// clamp to heightmap bounds
				//bound0 = max(bound0, int2(0));
				//bound1 = min(bound1, int2(MAP_SZ));

				shad->set_uniform("lod_bound0", (float2)bound0);
				shad->set_uniform("lod_bound1", (float2)bound1);

				// interate over all chunks in bounds
				for (int y=bound0.y; y<bound1.y; y+=sz)
				for (int x=bound0.x; x<bound1.x; x+=sz) {
					// exclude chunks that are already covered by smaller LOD chunks
					if (  x >= prev_bound0.x && x < prev_bound1.x &&
						  y >= prev_bound0.y && y < prev_bound1.y )
						continue;
					
					if (dbg)
						g.dbgdraw.wire_quad(float3((float2)int2(x,y), 0), (float2)(float)sz, DebugDraw::COLS[wrap(lod, ARRLEN(DebugDraw::COLS))]);

					// draw chunk with this lod
					shad->set_uniform("offset", float2(int2(x,y)));
					shad->set_uniform("quad_size", quad_size);
					
					chunk();

					drawn_chunks++;
				}

				prev_bound0 = bound0;
				prev_bound1 = bound1;
			}
		}

		void gen_terrain_quad () {
			
			constexpr int sz = TERRAIN_CHUNK_SZ;
			
			std::vector<TerrainVertex> verts( (sz+1) * (sz+1) );
			std::vector<uint16_t>      indices( sz * sz * 6 );

			int vert_count=0;
			for (int y=0; y<sz+1; ++y)
			for (int x=0; x<sz+1; ++x) {
				verts[vert_count++] = TerrainVertex{{ (float)x, (float)y }};
			}

			int idx_count = 0;
			for (int y=0; y<sz; ++y)
			for (int x=0; x<sz; ++x) {
				auto func = ((x ^ y) & 1) == 0 ? push_quad : push_quad2;

				func(&indices[idx_count],
					(uint16_t)( (y  )*(sz+1) + (x  ) ),
					(uint16_t)( (y  )*(sz+1) + (x+1) ),
					(uint16_t)( (y+1)*(sz+1) + (x+1) ),
					(uint16_t)( (y+1)*(sz+1) + (x  ) )
				);

				idx_count += 6;
			}

			terrain_chunk.upload(verts, indices);
			chunk_vertices = vert_count;
			chunk_indices = idx_count;
		}
		void render (Game& g, Renderer& r) {
			ZoneScoped;

			drawn_chunks = 0;

			if (draw_terrain && shad_terrain->prog) {

				OGL_TRACE("draw_terrain");
				
				PipelineState s;
				s.depth_test = true;
				s.blend_enable = false;
				r.state.set(s);

				glUseProgram(shad_terrain->prog);

				r.state.bind_textures(shad_terrain, {
					{ "clouds", r.clouds, r.sampler_normal },
					{"heightmap", heightmap, r.sampler_heightmap},
					{"terrain_diffuse", terrain_diffuse, r.sampler_normal},
				});
			
				shad_terrain->set_uniform("inv_max_size", 1.0f / float2((float)MAP_SZ));

				glBindVertexArray(terrain_chunk.vao);
			
				lodded_chunks(g, shad_terrain, 0, false, [this] () {
					glDrawElements(GL_TRIANGLES, chunk_indices, GL_UNSIGNED_SHORT, (void*)0);
				});
			}
			
			if (draw_water && shad_water->prog) {

				OGL_TRACE("draw_water");
				
				PipelineState s;
				s.depth_test = true;
				s.blend_enable = true;
				r.state.set(s);

				glUseProgram(shad_water->prog);

				r.state.bind_textures(shad_water, {
					{ "clouds", r.clouds, r.sampler_normal },
				});
				
				r.ocean.set_uniforms(shad_water);
				shad_water->set_uniform("inv_max_size", 1.0f / float2((float)MAP_SZ));

				glBindVertexArray(terrain_chunk.vao);
			
				lodded_chunks(g, shad_water, water_base_lod, dbg_lod, [this] () {
					glDrawElements(GL_TRIANGLES, chunk_indices, GL_UNSIGNED_SHORT, (void*)0);
				});
			}
		}
	};
	TerrainRenderer terrain_renderer;

	Shader* shad_postprocess = g_shaders.compile("postprocess");

	void render (Window& window, Game& g, int2 window_size) {
		ZoneScoped;

		{
			//OGL_TRACE("set state defaults");

			state.wireframe          = debug_draw.wireframe;
			state.wireframe_no_cull  = debug_draw.wireframe_no_cull;
			state.wireframe_no_blend = debug_draw.wireframe_no_blend;

			state.set_default();

			glEnable(GL_LINE_SMOOTH);
			glLineWidth(debug_draw.line_width);
		}

		lighting.sun_dir = float4(g.sun_dir, 0.0);
		ocean.update(window.input);

		{
			common_ubo.set(g.view, lighting);
			
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, debug_draw.indirect_vbo);

			debug_draw.update(window.input);
		}
		
		fbo.update(window_size);
		// Draw to MSAA float framebuffer
		fbo.bind();
		{
			glViewport(0, 0, fbo.renderscale.size.x, fbo.renderscale.size.y);

			glClearColor(0.01f, 0.02f, 0.03f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			terrain_renderer.render(g, *this);
		
			skybox.draw_skybox_last(state, *this);

			debug_draw.render(state, g.dbgdraw);
		}

		// 
		fbo.resolve(window_size);

		glViewport(0, 0, window_size.x, window_size.y);
		{
			if (shad_postprocess->prog) {
				OGL_TRACE("postprocess");
				
				glUseProgram(shad_postprocess->prog);
				
				shad_postprocess->set_uniform("exposure", exposure);

				state.bind_textures(shad_postprocess, {
					{ "fbo_col", { GL_TEXTURE_2D, fbo.fbo.col }, fbo.get_sampler() }
				});
				draw_fullscreen_triangle();
			}
		}

		{
		
			if (window.trigger_screenshot && !window.screenshot_hud) take_screenshot(window_size);
		
			// draw HUD

			window.draw_imgui();

			if (window.trigger_screenshot && window.screenshot_hud)  take_screenshot(window_size);
			window.trigger_screenshot = false;
		}
	}
};

}