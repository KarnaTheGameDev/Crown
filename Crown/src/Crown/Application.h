#pragma once

#include "Core.h"
#include "Window.h"
#include "Events/Event.h"
#include "Events/ApplicationEvent.h"

namespace Crown {

	class Shader;
	class OrthographicCamera;
	class Texture2D;

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
		bool OnWindowResize(WindowResizeEvent& e);

		std::unique_ptr<Window> m_Window;
		bool m_Running = true;

		// ponytail: raw GL handles for two hardcoded meshes. Buffer/VertexArray
		// classes go in when meshes stop being hardcoded, not before.
		unsigned int m_TriangleVA = 0, m_TriangleVB = 0, m_TriangleIB = 0;
		unsigned int m_SquareVA = 0, m_SquareVB = 0, m_SquareIB = 0;

		// Driven by the debug UI. Plain floats so glm stays out of this header.
		float m_ClearColor[3] = { 0.1f, 0.1f, 0.15f };
		float m_CameraSpeed = 1.5f;

		std::unique_ptr<Shader> m_Shader;
		std::unique_ptr<OrthographicCamera> m_Camera;
		std::unique_ptr<Texture2D> m_Texture;
		std::unique_ptr<Texture2D> m_WhiteTexture;

		static Application* s_Instance;
	};

	// To be defined in client
	Application* CreateApplication();
}
