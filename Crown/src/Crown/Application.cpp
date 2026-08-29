#include "crpch.h"
#include "Application.h"

#include "Crown/Log.h"
#include "Crown/Renderer/Shader.h"
#include "Crown/Renderer/OrthographicCamera.h"
#include "Crown/Renderer/Texture2D.h"
#include "Crown/Renderer/Framebuffer.h"

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

	// The sprite atlas is a square grid of this many cells per side.
	static constexpr int s_AtlasCells = 4;

	namespace {

		// The exe may be launched from bin/, from the debugger, or from a shipped
		// folder, and each gives a different working directory. Anchor on the
		// executable and walk up until assets/ turns up, so asset paths never
		// depend on how the program was started. A shipped build with assets/
		// beside the exe matches on the first iteration.
		void SetWorkingDirectoryToAssetRoot()
		{
			namespace fs = std::filesystem;

			wchar_t exePath[MAX_PATH]{};
			if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0)
			{
				CROWN_CORE_WARN("Could not locate the executable; leaving working directory alone");
				return;
			}

			std::error_code ec;
			fs::path dir = fs::path(exePath).parent_path();
			while (true)
			{
				if (fs::exists(dir / "assets", ec))
				{
					fs::current_path(dir, ec);
					if (ec)
						CROWN_CORE_WARN("Found assets at '{0}' but could not enter it", dir.string());
					else
						CROWN_CORE_INFO("Asset root: {0}", dir.string());
					return;
				}

				fs::path parent = dir.parent_path();
				if (parent == dir)          // reached the drive root
					break;
				dir = parent;
			}

			CROWN_CORE_WARN("No 'assets' directory found above the executable; textures will not load");
		}

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

		SetWorkingDirectoryToAssetRoot();

		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(CROWN_BIND_EVENT_FN(Application::OnEvent));

		float quadVerts[] = {
			// position              colour                  uv
			-0.75f, -0.75f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f,
			 0.75f, -0.75f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 0.0f,
			 0.75f,  0.75f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 1.0f,
			-0.75f,  0.75f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 1.0f
		};
		unsigned int quadIndices[] = { 0, 1, 2, 2, 3, 0 };
		auto quad = CreateMesh(quadVerts, sizeof(quadVerts), quadIndices, sizeof(quadIndices));
		m_QuadVA = quad.VA; m_QuadVB = quad.VB; m_QuadIB = quad.IB;

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

			// xy = atlas cell origin, zw = cell size, both in UV space.
			// (0,0,1,1) samples the whole texture.
			uniform vec4 u_TexRect;
			uniform vec4 u_Tint;

			out vec4 color;
			void main()
			{
				// Binding a 1x1 white texture makes this pure vertex colour, so
				// textured and untextured geometry share one shader.
				vec2 uv = u_TexRect.xy + v_TexCoord * u_TexRect.zw;
				color = texture(u_Texture, uv) * vec4(v_Color, 1.0) * u_Tint;
			}
		)";

		m_Shader = std::make_unique<Shader>(vertexSrc, fragmentSrc);
		m_Shader->Bind();
		m_Shader->SetInt("u_Texture", 0);

		m_Texture = std::make_unique<Texture2D>("assets/textures/atlas.png");
		
		// The checkerboard has alpha, so blending has to be on for it to read
		// as a texture rather than a black-fringed block.
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		m_Framebuffer = std::make_unique<Framebuffer>(m_ViewportWidth, m_ViewportHeight);

		float aspect = (float)m_ViewportWidth / (float)m_ViewportHeight;
		m_Camera = std::make_unique<OrthographicCamera>(
			-aspect * s_CameraZoom, aspect * s_CameraZoom, -s_CameraZoom, s_CameraZoom);

		// ImGui chains onto the GLFW callbacks WindowsWindow already installed,
		// so the engine's own event system keeps receiving everything.
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGui::StyleColorsDark();
		ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)m_Window->GetNativeWindow(), true);
		ImGui_ImplOpenGL3_Init("#version 330");

		// Seed a scene. Same grid as before, but as data that can be edited.
		for (int y = 0; y < 12; y++)
		{
			for (int x = 0; x < 20; x++)
			{
				Entity e;
				e.Name = "Sprite " + std::to_string(y * 20 + x);
				e.Position = { x * 0.15f - 1.425f, y * 0.15f - 0.825f, 0.0f };
				e.Scale = { 0.09f, 0.09f };
				e.AtlasCell = (x + y * 3) % (s_AtlasCells * s_AtlasCells);
				m_Entities.push_back(e);
			}
		}

		CROWN_CORE_INFO("WASD moves the camera, Q/E rotates it.");
	}

	Application::~Application()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glDeleteBuffers(1, &m_QuadIB);
		glDeleteBuffers(1, &m_QuadVB);
		glDeleteVertexArrays(1, &m_QuadVA);
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
		const float rotateSpeed = 90.0f;   // degrees per second

		float lastTime = (float)glfwGetTime();

		while (m_Running)
		{
			float time = (float)glfwGetTime();
			float dt = time - lastTime;
			lastTime = time;

			// Track the viewport panel. Resize is a no-op when nothing changed.
			if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
			{
				m_Framebuffer->Resize(m_ViewportWidth, m_ViewportHeight);
				float aspect = (float)m_ViewportWidth / (float)m_ViewportHeight;
				m_Camera->SetProjection(-aspect * s_CameraZoom, aspect * s_CameraZoom,
				                        -s_CameraZoom, s_CameraZoom);
			}

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

			// Scene renders into the framebuffer; the window itself only ever
			// shows ImGui.
			m_Framebuffer->Bind();
			glClearColor(m_ClearColor[0], m_ClearColor[1], m_ClearColor[2], 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			m_Shader->Bind();
			m_Shader->SetMat4("u_ViewProjection", m_Camera->GetViewProjection());

			glBindVertexArray(m_QuadVA);
			m_Texture->Bind(0);

			const float cw = 1.0f / s_AtlasCells;
			for (const Entity& e : m_Entities)
			{
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), e.Position)
					* glm::rotate(glm::mat4(1.0f), glm::radians(e.Rotation), glm::vec3(0, 0, 1))
					* glm::scale(glm::mat4(1.0f), glm::vec3(e.Scale, 1.0f));
				m_Shader->SetMat4("u_Transform", transform);

				// Atlas row 0 is the texture's top row, but v = 0 is its bottom.
				int cell = e.AtlasCell % (s_AtlasCells * s_AtlasCells);
				float u = (cell % s_AtlasCells) * cw;
				float v = 1.0f - cw - (cell / s_AtlasCells) * cw;
				m_Shader->SetFloat4("u_TexRect", { u, v, cw, cw });
				m_Shader->SetFloat4("u_Tint", e.Tint);

				glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
			}

			m_Framebuffer->Unbind();
			glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			ImGui::DockSpaceOverViewport();
			{
				const glm::vec3& pos = m_Camera->GetPosition();
				ImGui::SetNextWindowPos(ImVec2(1012.0f, 30.0f), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize(ImVec2(250.0f, 200.0f), ImGuiCond_FirstUseEver);
				ImGui::Begin("Crown");
				ImGui::Text("%.1f FPS (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
				ImGui::Separator();
				ImGui::ColorEdit3("Clear colour", m_ClearColor);
				ImGui::SliderFloat("Camera speed", &m_CameraSpeed, 0.1f, 5.0f);
				ImGui::Text("Camera  x %.2f  y %.2f  rot %.1f", pos.x, pos.y, m_Camera->GetRotation());
				ImGui::Text("Entities: %zu", m_Entities.size());
				ImGui::Text("Viewport %ux%u", m_ViewportWidth, m_ViewportHeight);
				ImGui::End();

				ImGui::SetNextWindowPos(ImVec2(10.0f, 30.0f), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize(ImVec2(220.0f, 400.0f), ImGuiCond_FirstUseEver);
				ImGui::Begin("Hierarchy");
				if (ImGui::Button("Add"))
				{
					Entity e;
					e.Name = "Entity " + std::to_string(m_Entities.size());
					e.Scale = { 0.15f, 0.15f };
					m_Entities.push_back(e);
					m_Selected = (int)m_Entities.size() - 1;
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete") && m_Selected >= 0)
				{
					m_Entities.erase(m_Entities.begin() + m_Selected);
					// Deleting the last row would leave the index past the end.
					m_Selected = m_Entities.empty() ? -1 : std::min(m_Selected, (int)m_Entities.size() - 1);
				}
				ImGui::Separator();

				for (int i = 0; i < (int)m_Entities.size(); i++)
				{
					// Names repeat, so the index disambiguates the widget id.
					ImGui::PushID(i);
					if (ImGui::Selectable(m_Entities[i].Name.c_str(), m_Selected == i))
						m_Selected = i;
					ImGui::PopID();
				}
				ImGui::End();

				ImGui::SetNextWindowPos(ImVec2(10.0f, 440.0f), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize(ImVec2(220.0f, 250.0f), ImGuiCond_FirstUseEver);
				ImGui::Begin("Properties");
				if (m_Selected >= 0 && m_Selected < (int)m_Entities.size())
				{
					Entity& e = m_Entities[m_Selected];

					char name[128];
					std::snprintf(name, sizeof(name), "%s", e.Name.c_str());
					if (ImGui::InputText("Name", name, sizeof(name)))
						e.Name = name;

					ImGui::DragFloat3("Position", &e.Position.x, 0.01f);
					ImGui::DragFloat("Rotation", &e.Rotation, 1.0f);
					ImGui::DragFloat2("Scale", &e.Scale.x, 0.005f, 0.001f, 10.0f);
					ImGui::SliderInt("Atlas cell", &e.AtlasCell, 0, s_AtlasCells * s_AtlasCells - 1);
					ImGui::ColorEdit4("Tint", &e.Tint.x);
				}
				else
				{
					ImGui::TextDisabled("Nothing selected");
				}
				ImGui::End();

				// No padding, or the scene sits inset from the panel edge and the
				// size we measure does not match what is displayed.
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				// Without an explicit first size this window collapses: it sizes
				// itself to its content, and its content is sized to the space
				// available inside it.
				ImGui::SetNextWindowPos(ImVec2(240.0f, 30.0f), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize(ImVec2(760.0f, 470.0f), ImGuiCond_FirstUseEver);
				ImGui::Begin("Viewport");
				ImVec2 avail = ImGui::GetContentRegionAvail();
				m_ViewportWidth  = (unsigned int)(avail.x > 0.0f ? avail.x : 0.0f);
				m_ViewportHeight = (unsigned int)(avail.y > 0.0f ? avail.y : 0.0f);

				// v is flipped: GL's first texel row is the bottom of the image.
				ImGui::Image((ImTextureID)m_Framebuffer->GetColorAttachment(),
				             avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
				ImGui::End();
				ImGui::PopStyleVar();
			}
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

} // namespace Crown
