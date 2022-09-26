#include "common.hpp"

#include "game.hpp"
#include "opengl/renderer.hpp"

struct App : IApp {
	SERIALIZE(App, game)

	virtual ~App () {}
	
	virtual void json_load () { load("debug.json", this); }
	virtual void json_save () { save("debug.json", *this); }

	Window window;

	Game game;
	ogl::Renderer r;

	virtual void frame (Window& window) {
		ZoneScoped;
		
		game.imgui();
		r.imgui();

		game.update(window);
		r.render(game, window.input.window_size);
	}
};

IApp* make_game () { return new App(); };
int main () {
	return run_game(make_game, "Voxel Ships");
}