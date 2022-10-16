#pragma once
#include "opengl.hpp"
#include "dbgdraw.hpp"

namespace ogl {

struct glDebugDraw {
	bool wireframe          = false;
	bool wireframe_no_cull  = false;
	bool wireframe_no_blend = true;

	float line_width = 1;
	
	Shader* shad_lines = g_shaders.compile("dbg_lines");
	Shader* shad_tris  = g_shaders.compile("dbg_tris");

	struct LineVertex {
		float3 pos;
		float4 col;

		ATTRIBUTES {
			ATTRIB(idx++, GL_FLOAT,3, LineVertex, pos);
			ATTRIB(idx++, GL_FLOAT,4, LineVertex, col);
		}
	};
	struct TriVertex {
		float3 pos;
		float3 norm;
		float4 col;

		ATTRIBUTES {
			ATTRIB(idx++, GL_FLOAT,3, TriVertex, pos);
			ATTRIB(idx++, GL_FLOAT,3, TriVertex, norm);
			ATTRIB(idx++, GL_FLOAT,4, TriVertex, col);
		}
	};

	VertexBuffer vbo_lines = vertex_buffer<LineVertex>("Dbg.LineVertex");
	VertexBuffer vbo_tris  = vertex_buffer<TriVertex>("Dbg.TriVertex");


	//VertexBufferInstancedI vbo_wire_cube   = indexed_instanced_buffer<DebugDraw::PosVertex, DebugDraw::Instance>("DebugDraw.vbo_wire_cube");
	//VertexBufferInstancedI vbo_wire_sphere = indexed_instanced_buffer<DebugDraw::PosVertex, DebugDraw::Instance>("DebugDraw.vbo_wire_sphere");
	//
	//struct IndirectLineVertex {
	//	float4 pos; // vec4 for std430 alignment
	//	float4 col;
	//
	//	template <typename ATTRIBS>
	//	static void attributes (ATTRIBS& a) {
	//		int loc = 0;
	//		a.init(sizeof(IndirectLineVertex));
	//		a.template add<AttribMode::FLOAT, decltype(pos)>(loc++, "pos", offsetof(IndirectLineVertex, pos));
	//		a.template add<AttribMode::FLOAT, decltype(col)>(loc++, "col", offsetof(IndirectLineVertex, col));
	//	}
	//};
	//struct IndirectWireInstace {
	//	float4 pos; // vec4 for std430 alignment
	//	float4 size; // vec4 for std430 alignment
	//	float4 col;
	//
	//	template <typename ATTRIBS>
	//	static void attributes (ATTRIBS& a) {
	//		a.init(sizeof(IndirectWireInstace), true);
	//		a.template add<AttribMode::FLOAT, decltype(pos)>(1, "instance_pos", offsetof(IndirectWireInstace, pos));
	//		a.template add<AttribMode::FLOAT, decltype(size)>(2, "instance_size", offsetof(IndirectWireInstace, size));
	//		a.template add<AttribMode::FLOAT, decltype(col)>(3, "instance_col", offsetof(IndirectWireInstace, col));
	//	}
	//};
	
	//struct IndirectBuffer {
	//	struct Lines {
	//		glDrawArraysIndirectCommand cmd;
	//		IndirectLineVertex vertices[4096];
	//	} lines;
	//
	//	//struct WireInstances {
	//	//	glDrawElementsIndirectCommand cmd;
	//	//	uint32_t _pad[3]; // padding for std430 alignment
	//	//	IndirectWireInstace vertices[4096];
	//	//};
	//	//
	//	//WireInstances wire_cubes;
	//	//WireInstances wire_spheres;
	//};
	//
	//// indirect_vbo contains all indirect drawing data
	//Vbo indirect_vbo = {"DebugDraw.indirect_draw"}; // = 1x IndirectBuffer
	
	//Vao indirect_lines_vao = setup_vao<IndirectLineVertex>("DebugDraw.indirect_lines", { indirect_vbo, offsetof(IndirectBuffer, lines.vertices[0]) });
	//
	//Vao indirect_wire_cube_vao = setup_instanced_vao<DebugDraw::PosVertex, IndirectWireInstace>("DebugDraw.indirect_wire_cubes", 
	//	{ indirect_vbo, offsetof(IndirectBuffer, wire_cubes.vertices[0]) },
	//	vbo_wire_cube.mesh_vbo, vbo_wire_cube.mesh_ebo);
	//
	//Vao indirect_wire_sphere_vao = setup_instanced_vao<DebugDraw::PosVertex, IndirectWireInstace>("DebugDraw.indirect_wire_spheres", 
	//	{ indirect_vbo, offsetof(IndirectBuffer, wire_spheres.vertices[0]) },
	//	vbo_wire_sphere.mesh_vbo, vbo_wire_sphere.mesh_ebo);
	//
	//int wire_sphere_indices_count;
	//
	//void clear_indirect () {
	//	glBindBuffer(GL_ARRAY_BUFFER, indirect_vbo);
	//
	//	{
	//		glDrawArraysIndirectCommand cmd = {};
	//		cmd.instanceCount = 1;
	//		glBufferSubData(GL_ARRAY_BUFFER, offsetof(IndirectBuffer, lines.cmd), sizeof(cmd), &cmd);
	//	}
	//
	//	{
	//		glDrawElementsIndirectCommand cmd = {};
	//		cmd.count = ARRLEN(DebugDraw::_wire_cube_indices);
	//		glBufferSubData(GL_ARRAY_BUFFER, offsetof(IndirectBuffer, wire_cubes.cmd), sizeof(cmd), &cmd);
	//	}
	//
	//	{
	//		glDrawElementsIndirectCommand cmd = {};
	//		cmd.count = wire_sphere_indices_count;
	//		glBufferSubData(GL_ARRAY_BUFFER, offsetof(IndirectBuffer, wire_spheres.cmd), sizeof(cmd), &cmd);
	//	}
	//
	//	glBindBuffer(GL_ARRAY_BUFFER, 0);
	//}
	
	bool update_indirect = false;
	bool accum_indirect = false;
	bool _clear_indirect = false;


	void imgui () {
		if (ImGui::TreeNode("Debug Draw")) {

			ImGui::Checkbox("wireframe", &wireframe);
			ImGui::SameLine();
			ImGui::Checkbox("no cull", &wireframe_no_cull);
			ImGui::SameLine();
			ImGui::Checkbox("no blend", &wireframe_no_blend);

			ImGui::SliderFloat("line_width", &line_width, 1.0f, 8.0f);

			ImGui::TreePop();
		}
	}

	void render (StateManager& state, DebugDraw& dbg) {
		OGL_TRACE("debug_draw");

		ZoneScoped;
		
		if (shad_lines->prog) {
			OGL_TRACE("Dbg.Lines");

			vbo_lines.stream(dbg.lines);

			if (dbg.lines.size() > 0) {
			
				glUseProgram(shad_lines->prog);

				PipelineState s;
				s.depth_test = false;
				s.blend_enable = true;
				state.set_no_override(s);

				glBindVertexArray(vbo_lines.vao);
				glDrawArrays(GL_LINES, 0, (GLsizei)dbg.lines.size());
			}
		}

		if (shad_tris->prog) {
			OGL_TRACE("Dbg.Tris");
		
			vbo_tris.stream(dbg.lines);

			if (dbg.lines.size() > 0) {
			
				glUseProgram(shad_tris->prog);

				PipelineState s;
				s.depth_test = false;
				s.blend_enable = true;
				state.set(s);

				glBindVertexArray(vbo_tris.vao);
				glDrawArrays(GL_TRIANGLES, 0, (GLsizei)dbg.lines.size());
			}
		}
	}
};

}
