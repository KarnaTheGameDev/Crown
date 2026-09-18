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

		// The step writes body transforms onto the entities, so an entity moved
		// by hand while playing is overwritten by the next one. These go the
		// other way, into the simulation, and are how a game drives a body.
		// All of them ignore an entity that has no body.
		void SetVelocity(uint32_t entityID, glm::vec2 velocity);
		glm::vec2 GetVelocity(uint32_t entityID) const;

		// Mass-dependent: the same impulse moves a small body further than a
		// big one. Use SetVelocity when you want an exact speed regardless.
		void ApplyImpulse(uint32_t entityID, glm::vec2 impulse);

		void SetTransform(uint32_t entityID, glm::vec2 position, float rotationDegrees);

		// Takes effect on the next step. Ignored when nothing is playing, so
		// the caller keeps whatever the next Begin should start with.
		void SetGravity(glm::vec2 gravity);

		bool IsRunning() const { return m_Running; }
		int BodyCount() const { return (int)m_Bodies.size(); }

	private:
		bool m_Running = false;

		// Box2D handles are small structs rather than pointers in v3, so they
		// are stored by value. Kept opaque here to avoid leaking box2d headers
		// into everything that includes Application.h.
		struct BodyHandle { uint64_t Bits[2]; };
		std::unordered_map<uint32_t, BodyHandle> m_Bodies;

		// Null when the entity has no body, or when nothing is playing.
		const BodyHandle* FindHandle(uint32_t entityID) const;

		// A fixed step keeps the simulation stable and repeatable. A frame that
		// took too long is spread over several steps instead of one huge one.
		float m_Accumulator = 0.0f;
	};

}
