#include "crpch.h"
#include "Application.h"

#include "Crown/Log.h"

// ponytail: glClear* are GL 1.1, straight out of opengl32.
// Add glad when the first shader needs GL 2.0+ entry points.
#include <GL/gl.h>

namespace Crown {

	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		CROWN_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(CROWN_BIND_EVENT_FN(Application::OnEvent));
	}

	Application::~Application()
	{
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(CROWN_BIND_EVENT_FN(Application::OnWindowClose));

		CROWN_CORE_TRACE("{0}", e.ToString());
	}

	void Application::Run()
	{
		while (m_Running)
		{
			glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

} // namespace Crown
