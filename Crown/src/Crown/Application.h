#pragma once

#include "Core.h"
#include "Window.h"
#include "Events/Event.h"
#include "Events/ApplicationEvent.h"

namespace Crown {

	class CROWN_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		inline Window& GetWindow() { return *m_Window; }

		inline static Application& Get() { return *s_Instance; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);

		std::unique_ptr<Window> m_Window;
		bool m_Running = true;

		// ponytail: raw GL handles while there is exactly one thing to draw.
		// Extract Shader/VertexBuffer/IndexBuffer once a second mesh exists.
		unsigned int m_VertexArray = 0, m_VertexBuffer = 0, m_IndexBuffer = 0, m_Shader = 0;

		static Application* s_Instance;
	};

	// To be defined in client
	Application* CreateApplication();
}
