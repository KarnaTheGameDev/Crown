#pragma once

#include "Core.h"
#include "Window.h"
#include "Events/Event.h"
#include "Events/ApplicationEvent.h"
#include "Scene/Entity.h"

#include <vector>

namespace Crown {

	class Shader;
	class OrthographicCamera;
	class Texture2D;
	class Framebuffer;

	class CROWN_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		// Overridden by the client to run its own code each frame.
		virtual void OnUpdate(float deltaTime) {}

		// Called inside the editor's ImGui frame so the client can draw its own
		// panels. Same shape as OnUpdate: the client needed somewhere to put a
		// score, and it had none.
		virtual void OnImGuiRender() {}

		// Creates an entity with a fresh id and returns it. Use this rather than
		// pushing onto GetEntities(), or the entity has no id to refer to later.
		Entity& CreateEntity(const std::string& name = "Entity");

		// Null once the entity is deleted. Look up every frame rather than
		// caching the pointer: the vector reallocates as entities are added.
		Entity* FindEntity(uint32_t id);

		// First entity whose bounds overlap this one, or null. Bounds are
		// axis-aligned, so a rotated quad is tested by its bounding box.
		Entity* FindOverlapping(const Entity& entity);

		// Invalidates every Entity pointer held across the call, since the
		// vector shifts. Returns false if the id was already gone.
		bool DestroyEntity(uint32_t id);

		std::vector<Entity>& GetEntities() { return m_Entities; }

		void OnEvent(Event& e);

		inline Window& GetWindow() { return *m_Window; }

		inline static Application& Get() { return *s_Instance; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);
		void LoadSceneFromDisk();

		std::unique_ptr<Window> m_Window;
		bool m_Running = true;

		// ponytail: one shared quad mesh, raw GL handles. A VertexArray class
		// goes in when there is more than one mesh again.
		unsigned int m_QuadVA = 0, m_QuadVB = 0, m_QuadIB = 0;

		std::vector<Entity> m_Entities;
		int m_Selected = -1;               // index into m_Entities, -1 for none
		uint32_t m_NextEntityID = 1;       // 0 is reserved for 'unassigned'
		bool m_DraggingEntity = false;     // drag must have started on the viewport

		// Driven by the debug UI. Plain floats so glm stays out of this header.
		float m_ClearColor[3] = { 0.1f, 0.1f, 0.15f };
		float m_CameraSpeed = 1.5f;

		std::unique_ptr<Shader> m_Shader;
		std::unique_ptr<OrthographicCamera> m_Camera;
		std::unique_ptr<Texture2D> m_Texture;
		std::unique_ptr<Framebuffer> m_Framebuffer;

		// Size of the viewport panel, which now drives the render target
		// and the camera aspect instead of the window.
		unsigned int m_ViewportWidth = 1280, m_ViewportHeight = 720;

		static Application* s_Instance;
	};

	// To be defined in client
	Application* CreateApplication();
}
