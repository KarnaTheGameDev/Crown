#pragma once

#include "Core.h"
#include "KeyCodes.h"

namespace Crown {

	// Lets client code read the keyboard without including GLFW. Application
	// polled GLFW directly while it was the only caller; the client is the
	// second, which is what makes this worth having.
	class CROWN_API Input
	{
	public:
		static bool IsKeyPressed(KeyCode key);
	};

}
