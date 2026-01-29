#include "Application.h"

namespace Crown {
	Application::Application() {
		// Constructor implementation
	}
	Application::~Application() {
		// Destructor implementation
	}

	void Application::Run() {
		bool running = true;
		while (running) {
			// Application main loop body
			// exit immediately to avoid hanging while debugging
			running = false;
		}
	}
} // namespace Crown
