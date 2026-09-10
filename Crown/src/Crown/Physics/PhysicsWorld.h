#pragma once

#include "Crown/Scene/Entity.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Crown {

	// Wraps Box2D for the lifetime of a play session. Bodies are built from the
	// entities when play starts and destroyed when it stops, so the simulation
	// never touches a scene that is being edited.
	class PhysicsWorld
	{
	public:
		struct Contact
		{
			uint32_t A = 0, B = 0;
			bool Began = true;         // false when the two separated
		};

		~PhysicsWorld();

		PhysicsWorld(const PhysicsWorld&) = delete;
		PhysicsWorld& operator=(const PhysicsWorld&) = delete;
		PhysicsWorld() = default;

		// Builds a body for every entity whose Body is not None.
		void Begin(std::vector<Entity>& entities, glm::vec2 gravity);
		void End();

		// Steps the simulation and writes transforms back onto the entities.
		// Contacts that occurred during the step are appended to outContacts.
		void Step(float deltaTime, std::vector<Entity>& entities, std::vector<Contact>& outContacts);

		// Entities created or destroyed mid-play are not automatically tracked;
		// the client tells us. Destroy is safe for an id we never knew about.
		void AddBody(Entity& entity);
		void DestroyBody(uint32_t entityID);

		bool IsRunning() const { return m_Running; }
		int BodyCount() const { return (int)m_Bodies.size(); }

	private:
		bool m_Running = false;

		// Box2D handles are small structs rather than pointers in v3, so they
		// are stored by value. Kept opaque here to avoid leaking box2d headers
		// into everything that includes Application.h.
		struct BodyHandle { uint64_t Bits[2]; };
		std::unordered_map<uint32_t, BodyHandle> m_Bodies;

		// A fixed step keeps the simulation stable and repeatable. A frame that
		// took too long is spread over several steps instead of one huge one.
		float m_Accumulator = 0.0f;
	};

}
