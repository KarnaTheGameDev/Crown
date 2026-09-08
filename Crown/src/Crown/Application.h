#pragma once

#include "Core.h"
#include "Window.h"
#include "Events/Event.h"
#include "Events/ApplicationEvent.h"
#include "Scene/Entity.h"
#include "Renderer/TextureLibrary.h"

#include <glm/glm.hpp>

#include <vector>

namespace Crown {

	class Shader;
	class OrthographicCamera;
	class Texture2D;
	class Framebuffer;

	// Edit is the scene as authored. Play runs the game on a copy of it, and
	// Stop throws that copy away, so playing can never damage what you built.
	enum class SceneState { Edit, Play, Paused };

	class CROWN_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		// Overridden by the client to run its own code each frame.
		// Only called while playing, so a game cannot move things around while
		// they are being arranged.
		virtual void OnUpdate(float deltaTime) {}

		// Set up and tear down whatever the game needs. Entities created in
		// OnPlay are discarded on Stop along with everything else the game did,
		// so this is where a game builds its level rather than in its
		// constructor.
		virtual void OnPlay() {}
		virtual void OnStop() {}

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

		// Every entity whose bounds overlap this one, by id. Ids rather than
		// pointers, because acting on a hit usually destroys something and that
		// invalidates every pointer into the scene. `out` is cleared first, so
		// callers can keep one vector and reuse it each frame.
		//
		// Bounds are axis-aligned, so a rotated quad is tested by its box.
		void FindOverlapping(const Entity& entity, std::vector<uint32_t>& out) const;

		// Invalidates every Entity pointer held across the call, since the
		// vector shifts. Returns false if the id was already gone.
		bool DestroyEntity(uint32_t id);

		std::vector<Entity>& GetEntities() { return m_Entities; }

		SceneState GetSceneState() const { return m_SceneState; }

		void OnEvent(Event& e);

		inline Window& GetWindow() { return *m_Window; }

		inline static Application& Get() { return *s_Instance; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);
		void LoadSceneFromDisk();
		void StartPlaying();
		void StopPlaying();

		std::unique_ptr<Window> m_Window;
		bool m_Running = true;

		// ponytail: one shared quad mesh, raw GL handles. A VertexArray class
		// goes in when there is more than one mesh again.
		unsigned int m_QuadVA = 0, m_QuadVB = 0, m_QuadIB = 0;

		std::vector<Entity> m_Entities;

		SceneState m_SceneState = SceneState::Edit;
		// The scene as it was when Play was pressed. Restored on Stop.
		std::vector<Entity> m_EditSnapshot;
		uint32_t m_EditNextEntityID = 1;
		int m_Selected = -1;               // index into m_Entities, -1 for none
		glm::ivec2 m_SheetGrid{ 4, 4 };    // editor-only sprite sheet helper
		int m_SheetCell = 0;
		int m_AddCounter = 0;              // staggers newly added entities
		std::string m_Status;              // transient editor feedback
		float m_StatusTime = 0.0f;
		uint32_t m_NextEntityID = 1;       // 0 is reserved for 'unassigned'
		bool m_DraggingEntity = false;     // drag must have started on the viewport

		// Driven by the debug UI. Plain floats so glm stays out of this header.
		float m_ClearColor[3] = { 0.1f, 0.1f, 0.15f };
		float m_CameraSpeed = 1.5f;

		std::unique_ptr<Shader> m_Shader;
		std::unique_ptr<OrthographicCamera> m_Camera;
		TextureLibrary m_Textures;
		std::unique_ptr<Framebuffer> m_Framebuffer;

		// Size of the viewport panel, which now drives the render target
		// and the camera aspect instead of the window.
		unsigned int m_ViewportWidth = 1280, m_ViewportHeight = 720;

		static Application* s_Instance;
	};

	// To be defined in client
	Application* CreateApplication();
}
