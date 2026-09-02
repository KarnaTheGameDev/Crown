#include "crpch.h"
#include "Input.h"

#include "Application.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace Crown {

	bool Input::IsKeyPressed(KeyCode key)
	{
		// Typing into an editor field must not also drive the game. Every caller
		// wants this, so it lives here rather than in each of them.
		if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard)
			return false;

		auto* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		return window && glfwGetKey(window, key) == GLFW_PRESS;
	}

}
