#include "crpch.h"
#include "Application.h"

#include "Crown/Log.h"
#include "Crown/Renderer/Shader.h"
#include "Crown/Renderer/OrthographicCamera.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace Crown {

	Application* Application::s_Instance = nullptr;

	namespace {

		struct MeshHandles { unsigned int VA, VB, IB; };

		// Uploads one interleaved position+colour mesh. Layout is fixed at
		// vec3 position / vec3 colour because both meshes here use it.
		MeshHandles CreateMesh(const float* verts, size_t vertBytes,
		                       const unsigned int* indices, size_t indexBytes)
		{
			MeshHandles m{};

			glGenVertexArrays(1, &m.VA);
			glBindVertexArray(m.VA);

			glGenBuffers(1, &m.VB);
			glBindBuffer(GL_ARRAY_BUFFER, m.VB);
			glBufferData(GL_ARRAY_BUFFER, vertBytes, verts, GL_STATIC_DRAW);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void*)(3 * sizeof(float)));

			glGenBuffers(1, &m.IB);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.IB);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBytes, indices, GL_STATIC_DRAW);

			return m;
		}

	}

	Application::Application()
	{
		CROWN_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(CROWN_BIND_EVENT_FN(Application::OnEvent));

		float triangleVerts[] = {
			-0.5f, -0.5f, 0.0f,   0.9f, 0.2f, 0.3f,
			 0.5f, -0.5f, 0.0f,   0.2f, 0.8f, 0.4f,
			 0.0f,  0.5f, 0.0f,   0.3f, 0.4f, 0.9f
		};
		unsigned int triangleIndices[] = { 0, 1, 2 };
		auto tri = CreateMesh(triangleVerts, sizeof(triangleVerts), triangleIndices, sizeof(triangleIndices));
		m_TriangleVA = tri.VA; m_TriangleVB = tri.VB; m_TriangleIB = tri.IB;

		float squareVerts[] = {
			-0.75f, -0.75f, 0.0f,   0.25f, 0.28f, 0.34f,
			 0.75f, -0.75f, 0.0f,   0.25f, 0.28f, 0.34f,
			 0.75f,  0.75f, 0.0f,   0.20f, 0.22f, 0.28f,
			-0.75f,  0.75f, 0.0f,   0.20f, 0.22f, 0.28f
		};
		unsigned int squareIndices[] = { 0, 1, 2, 2, 3, 0 };
		auto square = CreateMesh(squareVerts, sizeof(squareVerts), squareIndices, sizeof(squareIndices));
		m_SquareVA = square.VA; m_SquareVB = square.VB; m_SquareIB = square.IB;

		const std::string vertexSrc = R"(
			#version 330 core
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec3 a_Color;

			uniform mat4 u_ViewProjection;

			out vec3 v_Color;
			void main()
			{
				v_Color = a_Color;
				gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
			}
		)";

		const std::string fragmentSrc = R"(
			#version 330 core
			in vec3 v_Color;
			out vec4 color;
			void main()
			{
				color = vec4(v_Color, 1.0);
			}
		)";

		m_Shader = std::make_unique<Shader>(vertexSrc, fragmentSrc);

		// 16:9 so the square does not stretch with the default window.
		m_Camera = std::make_unique<OrthographicCamera>(-1.6f, 1.6f, -0.9f, 0.9f);

		CROWN_CORE_INFO("WASD moves the camera, Q/E rotates it.");
	}

	Application::~Application()
	{
		glDeleteBuffers(1, &m_TriangleIB);
		glDeleteBuffers(1, &m_TriangleVB);
		glDeleteVertexArrays(1, &m_TriangleVA);

		glDeleteBuffers(1, &m_SquareIB);
		glDeleteBuffers(1, &m_SquareVB);
		glDeleteVertexArrays(1, &m_SquareVA);
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(CROWN_BIND_EVENT_FN(Application::OnWindowClose));
	}

	void Application::Run()
	{
		// ponytail: camera input polled straight from GLFW. An Input abstraction
		// earns its place when a second platform exists.
		GLFWwindow* window = (GLFWwindow*)m_Window->GetNativeWindow();
		const float moveSpeed = 1.5f;      // world units per second
		const float rotateSpeed = 90.0f;   // degrees per second

		float lastTime = (float)glfwGetTime();

		while (m_Running)
		{
			float time = (float)glfwGetTime();
			float dt = time - lastTime;
			lastTime = time;

			glm::vec3 position = m_Camera->GetPosition();
			if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position.x -= moveSpeed * dt;
			if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position.x += moveSpeed * dt;
			if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position.y -= moveSpeed * dt;
			if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position.y += moveSpeed * dt;
			m_Camera->SetPosition(position);

			float rotation = m_Camera->GetRotation();
			if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) rotation += rotateSpeed * dt;
			if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) rotation -= rotateSpeed * dt;
			m_Camera->SetRotation(rotation);

			glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			m_Shader->Bind();
			m_Shader->SetMat4("u_ViewProjection", m_Camera->GetViewProjection());

			glBindVertexArray(m_SquareVA);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

			glBindVertexArray(m_TriangleVA);
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
