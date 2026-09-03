#include "crpch.h"
#include "Application.h"

#include "Crown/Log.h"
#include "Crown/Renderer/Shader.h"
#include "Crown/Renderer/OrthographicCamera.h"
#include "Crown/Renderer/Texture2D.h"
#include "Crown/Renderer/Framebuffer.h"
#include "Crown/Scene/SceneSerializer.h"

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

	static constexpr const char* s_ScenePath = "assets/scenes/scene.crown";

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

		// The quad mesh spans -0.75..0.75 in model space; picking tests against
		// that, so it has to track the vertex data below.
		constexpr float s_QuadHalfExtent = 0.75f;

		glm::mat4 EntityTransform(const Entity& e)
		{
			return glm::translate(glm::mat4(1.0f), e.Position)
				* glm::rotate(glm::mat4(1.0f), glm::radians(e.Rotation), glm::vec3(0, 0, 1))
				* glm::scale(glm::mat4(1.0f), glm::vec3(e.Scale, 1.0f));
		}

		// World-space bounds of an entity's quad, as the box around its four
		// transformed corners.
		// ponytail: a rotated quad over-reports at the corners, so contact can
		// register slightly early. Swap in SAT if rotation needs to be exact.
		void EntityBounds(const Entity& e, glm::vec2& outMin, glm::vec2& outMax)
		{
			const glm::mat4 transform = EntityTransform(e);
			const float h = s_QuadHalfExtent;
			const glm::vec2 local[4] = { { -h, -h }, { h, -h }, { h, h }, { -h, h } };

			outMin = glm::vec2(std::numeric_limits<float>::max());
			outMax = glm::vec2(std::numeric_limits<float>::lowest());
			for (const glm::vec2& corner : local)
			{
				glm::vec2 world = glm::vec2(transform * glm::vec4(corner, 0.0f, 1.0f));
				outMin = glm::min(outMin, world);
				outMax = glm::max(outMax, world);
			}
		}

		// Screen pixel inside the viewport image -> world position on z = 0.
		glm::vec2 ViewportToWorld(ImVec2 pixel, ImVec2 origin, ImVec2 size, const glm::mat4& invViewProj)
		{
			glm::vec2 ndc(
				2.0f * (pixel.x - origin.x) / size.x - 1.0f,
				1.0f - 2.0f * (pixel.y - origin.y) / size.y);       // ImGui y grows down, NDC y grows up
			glm::vec4 world = invViewProj * glm::vec4(ndc, 0.0f, 1.0f);
			return glm::vec2(world);
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
		// ImGui drags a window from anywhere in its body by default. The viewport
		// image does not consume the drag, so dragging an entity would drag the
		// panel along with it.
		ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
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

			OnUpdate(dt);

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
				m_Shader->SetMat4("u_Transform", EntityTransform(e));

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
			OnImGuiRender();

			if (ImGui::GetIO().KeyCtrl)
			{
				if (ImGui::IsKeyPressed(ImGuiKey_S, false))
					SaveScene(m_Entities, s_ScenePath);
				if (ImGui::IsKeyPressed(ImGuiKey_O, false))
					LoadSceneFromDisk();
			}
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
				ImGui::Text("Entities: %zu   next id: %u", m_Entities.size(), m_NextEntityID);
				ImGui::Text("Viewport %ux%u", m_ViewportWidth, m_ViewportHeight);
				ImGui::End();

				ImGui::SetNextWindowPos(ImVec2(10.0f, 30.0f), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize(ImVec2(220.0f, 400.0f), ImGuiCond_FirstUseEver);
				ImGui::Begin("Hierarchy");
				if (ImGui::Button("Add"))
				{
					Entity& e = CreateEntity("Entity " + std::to_string(m_NextEntityID));
					e.Scale = { 0.15f, 0.15f };
					m_Selected = (int)m_Entities.size() - 1;
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete") && m_Selected >= 0)
				{
					m_Entities.erase(m_Entities.begin() + m_Selected);
					// Deleting the last row would leave the index past the end.
					m_Selected = m_Entities.empty() ? -1 : std::min(m_Selected, (int)m_Entities.size() - 1);
				}
				ImGui::SameLine();
				if (ImGui::Button("Save"))
					SaveScene(m_Entities, s_ScenePath);
				ImGui::SameLine();
				if (ImGui::Button("Load"))
					LoadSceneFromDisk();
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

				// Click to select, drag to move. Both work in world space via the
				// same inverse view-projection, so they stay correct under camera
				// pan, zoom and rotation without special-casing any of them.
				if (avail.x > 0.0f && avail.y > 0.0f)
				{
					ImVec2 origin = ImGui::GetItemRectMin();
					glm::mat4 invViewProj = glm::inverse(m_Camera->GetViewProjection());

					if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						glm::vec2 world = ViewportToWorld(ImGui::GetMousePos(), origin, avail, invViewProj);

						m_Selected = -1;
						// Back to front: the last drawn entity is the one on top.
						for (int i = (int)m_Entities.size() - 1; i >= 0; i--)
						{
							glm::vec4 local = glm::inverse(EntityTransform(m_Entities[i])) * glm::vec4(world, 0.0f, 1.0f);
							if (std::abs(local.x) <= s_QuadHalfExtent && std::abs(local.y) <= s_QuadHalfExtent)
							{
								m_Selected = i;
								break;
							}
						}
						m_DraggingEntity = m_Selected >= 0;
					}

					if (m_DraggingEntity && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
					{
						// Unproject both ends of the drag rather than scaling pixels
						// by a zoom factor, so a rotated camera still moves the
						// entity the direction the mouse went.
						ImVec2 now = ImGui::GetMousePos();
						ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
						ImVec2 before(now.x - delta.x, now.y - delta.y);

						glm::vec2 moved = ViewportToWorld(now, origin, avail, invViewProj)
						                - ViewportToWorld(before, origin, avail, invViewProj);

						if (m_Selected >= 0 && m_Selected < (int)m_Entities.size())
							m_Entities[m_Selected].Position += glm::vec3(moved, 0.0f);

						ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
					}

					if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
						m_DraggingEntity = false;
				}

				ImGui::End();
				ImGui::PopStyleVar();
			}
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			m_Window->OnUpdate();
		}
	}

	Entity& Application::CreateEntity(const std::string& name)
	{
		Entity e;
		e.ID = m_NextEntityID++;
		e.Name = name;
		m_Entities.push_back(e);
		return m_Entities.back();
	}

	Entity* Application::FindEntity(uint32_t id)
	{
		if (id == 0)
			return nullptr;
		for (Entity& e : m_Entities)
			if (e.ID == id)
				return &e;
		return nullptr;
	}

	Entity* Application::FindOverlapping(const Entity& entity)
	{
		glm::vec2 aMin, aMax;
		EntityBounds(entity, aMin, aMax);

		for (Entity& other : m_Entities)
		{
			// Compare by id, not by address: the caller's reference may have
			// come from a different lookup than this one.
			if (other.ID == entity.ID)
				continue;

			glm::vec2 bMin, bMax;
			EntityBounds(other, bMin, bMax);

			if (aMin.x <= bMax.x && aMax.x >= bMin.x &&
			    aMin.y <= bMax.y && aMax.y >= bMin.y)
				return &other;
		}
		return nullptr;
	}

	bool Application::DestroyEntity(uint32_t id)
	{
		for (size_t i = 0; i < m_Entities.size(); i++)
		{
			if (m_Entities[i].ID != id)
				continue;

			m_Entities.erase(m_Entities.begin() + i);

			// The editor's selection is an index, so it has to move with the
			// erase or it starts pointing at the wrong entity.
			if (m_Selected == (int)i)
				m_Selected = -1;
			else if (m_Selected > (int)i)
				m_Selected--;

			return true;
		}
		return false;
	}

	void Application::LoadSceneFromDisk()
	{
		if (!LoadScene(m_Entities, s_ScenePath))
			return;

		// Ids come back from the file, so the counter has to clear the highest
		// one or the next entity created would collide with a loaded one.
		// A scene written before ids existed has none, so assign those now.
		uint32_t highest = 0;
		for (const Entity& e : m_Entities)
			highest = std::max(highest, e.ID);
		m_NextEntityID = highest + 1;

		for (Entity& e : m_Entities)
			if (e.ID == 0)
				e.ID = m_NextEntityID++;

		m_Selected = -1;                 // indices referred to the old scene
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

} // namespace Crown
