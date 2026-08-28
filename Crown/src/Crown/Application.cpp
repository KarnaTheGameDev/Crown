#include "crpch.h"
#include "Application.h"

#include "Crown/Log.h"

#include <glad/gl.h>

namespace Crown {

	// Compiles one stage and logs the driver's message on failure. Returns 0 if it did not compile.
	static unsigned int CompileShader(unsigned int type, const char* src)
	{
		unsigned int id = glCreateShader(type);
		glShaderSource(id, 1, &src, nullptr);
		glCompileShader(id);

		int ok = 0;
		glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
		if (!ok)
		{
			int len = 0;
			glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
			std::vector<char> log(len);
			glGetShaderInfoLog(id, len, &len, log.data());
			CROWN_CORE_ERROR("Shader compile failed: {0}", log.data());
			glDeleteShader(id);
			return 0;
		}
		return id;
	}


	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		CROWN_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(CROWN_BIND_EVENT_FN(Application::OnEvent));

		// ponytail: one hardcoded triangle to prove the renderer works end to end.
		// Everything below moves behind a Renderer API once there is a second mesh.
		glGenVertexArrays(1, &m_VertexArray);
		glBindVertexArray(m_VertexArray);

		float vertices[] = {
			-0.5f, -0.5f, 0.0f,   0.9f, 0.2f, 0.3f,
			 0.5f, -0.5f, 0.0f,   0.2f, 0.8f, 0.4f,
			 0.0f,  0.5f, 0.0f,   0.3f, 0.4f, 0.9f
		};
		glGenBuffers(1, &m_VertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void*)(3 * sizeof(float)));

		unsigned int indices[] = { 0, 1, 2 };
		glGenBuffers(1, &m_IndexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		const char* vertexSrc = R"(
			#version 330 core
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec3 a_Color;
			out vec3 v_Color;
			void main()
			{
				v_Color = a_Color;
				gl_Position = vec4(a_Position, 1.0);
			}
		)";

		const char* fragmentSrc = R"(
			#version 330 core
			in vec3 v_Color;
			out vec4 color;
			void main()
			{
				color = vec4(v_Color, 1.0);
			}
		)";

		unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexSrc);
		unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
		if (vs && fs)
		{
			m_Shader = glCreateProgram();
			glAttachShader(m_Shader, vs);
			glAttachShader(m_Shader, fs);
			glLinkProgram(m_Shader);

			int ok = 0;
			glGetProgramiv(m_Shader, GL_LINK_STATUS, &ok);
			if (!ok)
			{
				int len = 0;
				glGetProgramiv(m_Shader, GL_INFO_LOG_LENGTH, &len);
				std::vector<char> log(len);
				glGetProgramInfoLog(m_Shader, len, &len, log.data());
				CROWN_CORE_ERROR("Shader link failed: {0}", log.data());
				glDeleteProgram(m_Shader);
				m_Shader = 0;
			}
		}
		glDeleteShader(vs);
		glDeleteShader(fs);
	}

	Application::~Application()
	{
		glDeleteProgram(m_Shader);
		glDeleteBuffers(1, &m_IndexBuffer);
		glDeleteBuffers(1, &m_VertexBuffer);
		glDeleteVertexArrays(1, &m_VertexArray);
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

			glUseProgram(m_Shader);
			glBindVertexArray(m_VertexArray);
			glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);

			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

} // namespace Crown
