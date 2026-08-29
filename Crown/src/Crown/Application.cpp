#include "crpch.h"
#include "Application.h"

#include "Crown/Log.h"
#include "Crown/Renderer/Shader.h"
#include "Crown/Renderer/OrthographicCamera.h"
#include "Crown/Renderer/Texture2D.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace Crown {

	Application* Application::s_Instance = nullptr;

	// Half-height of the visible world, in world units.
	static constexpr float s_CameraZoom = 0.9f;

	namespace {

		struct MeshHandles { unsigned int VA, VB, IB; };

		// Uploads one interleaved mesh. Layout is fixed at
		// vec3 position / vec3 colour / vec2 uv because both meshes use it.
		MeshHandles CreateMesh(const float* verts, size_t vertBytes,
		                       const unsigned int* indices, size_t indexBytes)
		{
			MeshHandles m{};

			glGenVertexArrays(1, &m.VA);
			glBindVertexArray(m.VA);

			glGenBuffers(1, &m.VB);
			glBindBuffer(GL_ARRAY_BUFFER, m.VB);
			glBufferData(GL_ARRAY_BUFFER, vertBytes, verts, GL_STATIC_DRAW);

			const GLsizei stride = 8 * sizeof(float);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(3 * sizeof(float)));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(6 * sizeof(float)));

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
			// position            colour             uv
			-0.5f, -0.5f, 0.0f,   0.9f, 0.2f, 0.3f,   0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f,   0.2f, 0.8f, 0.4f,   1.0f, 0.0f,
			 0.0f,  0.5f, 0.0f,   0.3f, 0.4f, 0.9f,   0.5f, 1.0f
		};
		unsigned int triangleIndices[] = { 0, 1, 2 };
		auto tri = CreateMesh(triangleVerts, sizeof(triangleVerts), triangleIndices, sizeof(triangleIndices));
		m_TriangleVA = tri.VA; m_TriangleVB = tri.VB; m_TriangleIB = tri.IB;

		float squareVerts[] = {
			// position              colour                  uv
			-0.75f, -0.75f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f,
			 0.75f, -0.75f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 0.0f,
			 0.75f,  0.75f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 1.0f,
			-0.75f,  0.75f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 1.0f
		};
		unsigned int squareIndices[] = { 0, 1, 2, 2, 3, 0 };
		auto square = CreateMesh(squareVerts, sizeof(squareVerts), squareIndices, sizeof(squareIndices));
		m_SquareVA = square.VA; m_SquareVB = square.VB; m_SquareIB = square.IB;

		const std::string vertexSrc = R"(
			#version 330 core
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec3 a_Color;
			layout(location = 2) in vec2 a_TexCoord;

			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;

			out vec3 v_Color;
			out vec2 v_TexCoord;
			void main()
			{
				v_Color = a_Color;
				v_TexCoord = a_TexCoord;
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
			}
		)";

		const std::string fragmentSrc = R"(
			#version 330 core
			in vec3 v_Color;
			in vec2 v_TexCoord;

			uniform sampler2D u_Texture;

			out vec4 color;
			void main()
			{
				// Binding a 1x1 white texture makes this pure vertex colour, so
				// textured and untextured geometry share one shader.
				color = texture(u_Texture, v_TexCoord) * vec4(v_Color, 1.0);
			}
		)";

		m_Shader = std::make_unique<Shader>(vertexSrc, fragmentSrc);
		m_Shader->Bind();
		m_Shader->SetInt("u_Texture", 0);

		m_Texture = std::make_unique<Texture2D>("assets/textures/checkerboard.png");
		m_WhiteTexture = std::make_unique<Texture2D>();

		// The checkerboard has alpha, so blending has to be on for it to read
		// as a texture rather than a black-fringed block.
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		float aspect = (float)m_Window->GetWidth() / (float)m_Window->GetHeight();
		m_Camera = std::make_unique<OrthographicCamera>(
			-aspect * s_CameraZoom, aspect * s_CameraZoom, -s_CameraZoom, s_CameraZoom);

		// ImGui chains onto the GLFW callbacks WindowsWindow already installed,
		// so the engine's own event system keeps receiving everything.
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)m_Window->GetNativeWindow(), true);
		ImGui_ImplOpenGL3_Init("#version 330");

		CROWN_CORE_INFO("WASD moves the camera, Q/E rotates it.");
	}

	Application::~Application()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

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
		dispatcher.Dispatch<WindowResizeEvent>(CROWN_BIND_EVENT_FN(Application::OnWindowResize));
	}

	void Application::Run()
	{
		// ponytail: camera input polled straight from GLFW. An Input abstraction
		// earns its place when a second platform exists.
		GLFWwindow* window = (GLFWwindow*)m_Window->GetNativeWindow();
		const float rotateSpeed = 90.0f;   // degrees per second

		float lastTime = (float)glfwGetTime();

		while (m_Running)
		{
			float time = (float)glfwGetTime();
			float dt = time - lastTime;
			lastTime = time;

			// Typing into a slider must not also drive the camera.
			if (!ImGui::GetIO().WantCaptureKeyboard)
			{
				glm::vec3 position = m_Camera->GetPosition();
				if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position.x -= m_CameraSpeed * dt;
				if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position.x += m_CameraSpeed * dt;
				if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position.y -= m_CameraSpeed * dt;
				if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position.y += m_CameraSpeed * dt;
				m_Camera->SetPosition(position);

				float rotation = m_Camera->GetRotation();
				if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) rotation += rotateSpeed * dt;
				if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) rotation -= rotateSpeed * dt;
				m_Camera->SetRotation(rotation);
			}

			glClearColor(m_ClearColor[0], m_ClearColor[1], m_ClearColor[2], 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			m_Shader->Bind();
			m_Shader->SetMat4("u_ViewProjection", m_Camera->GetViewProjection());

			// One mesh, many transforms: this is what the model matrix buys.
			// Quad spans 1.5 units, so scale 0.06 makes it 0.09 wide and the
			// 0.15 spacing leaves a visible gap. Grid fills the 3.2 x 1.8 view.
			glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.06f));
			m_Texture->Bind(0);
			glBindVertexArray(m_SquareVA);
			for (int y = 0; y < 12; y++)
			{
				for (int x = 0; x < 20; x++)
				{
					glm::vec3 pos(x * 0.15f - 1.425f, y * 0.15f - 0.825f, 0.0f);
					m_Shader->SetMat4("u_Transform", glm::translate(glm::mat4(1.0f), pos) * scale);
					glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
				}
			}

			m_Shader->SetMat4("u_Transform", glm::mat4(1.0f));
			m_WhiteTexture->Bind(0);         // white => triangle keeps pure vertex colour
			glBindVertexArray(m_TriangleVA);
			glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			{
				const glm::vec3& pos = m_Camera->GetPosition();
				ImGui::Begin("Crown");
				ImGui::Text("%.1f FPS (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
				ImGui::Separator();
				ImGui::ColorEdit3("Clear colour", m_ClearColor);
				ImGui::SliderFloat("Camera speed", &m_CameraSpeed, 0.1f, 5.0f);
				ImGui::Text("Camera  x %.2f  y %.2f  rot %.1f", pos.x, pos.y, m_Camera->GetRotation());
				ImGui::Text("Quads drawn: 240");
				ImGui::End();
			}
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		unsigned int w = e.GetWidth(), h = e.GetHeight();
		if (w == 0 || h == 0)   // minimised: nothing to draw into, and h == 0 would divide by zero
			return false;

		glViewport(0, 0, w, h);

		// Hold the vertical extent and widen horizontally, so geometry keeps its
		// shape instead of stretching with the window.
		float aspect = (float)w / (float)h;
		m_Camera->SetProjection(-aspect * s_CameraZoom, aspect * s_CameraZoom, -s_CameraZoom, s_CameraZoom);
		return false;   // let other listeners see the resize too
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

} // namespace Crown
