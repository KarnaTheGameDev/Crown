#include "crpch.h"
#include "PhysicsWorld.h"

#include <box2d/box2d.h>

namespace Crown {

	namespace {

		// Box2D v3 hands out small value handles rather than pointers. They fit
		// in the opaque BodyHandle in the header, which keeps box2d out of
		// every translation unit that includes Application.h.
		static_assert(sizeof(b2BodyId) <= sizeof(uint64_t) * 2, "BodyHandle too small for b2BodyId");
		static_assert(sizeof(b2WorldId) <= sizeof(uint64_t) * 2, "world handle too small");

		b2WorldId g_World = b2_nullWorldId;

		b2BodyType ToBox2D(Entity::BodyType type)
		{
			switch (type)
			{
				case Entity::BodyType::Static:    return b2_staticBody;
				case Entity::BodyType::Kinematic: return b2_kinematicBody;
				default:                          return b2_dynamicBody;
			}
		}

		// The entity id travels on the body, so a contact can be reported back
		// in the client's own terms rather than as a Box2D handle.
		uint32_t EntityIdOf(b2ShapeId shape)
		{
			b2BodyId body = b2Shape_GetBody(shape);
			return (uint32_t)(uintptr_t)b2Body_GetUserData(body);
		}

	}

	PhysicsWorld::~PhysicsWorld()
	{
		End();
	}

	void PhysicsWorld::Begin(std::vector<Entity>& entities, glm::vec2 gravity)
	{
		End();

		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity = { gravity.x, gravity.y };
		g_World = b2CreateWorld(&worldDef);

		m_Running = true;
		m_Accumulator = 0.0f;

		for (Entity& e : entities)
			AddBody(e);

		CROWN_CORE_INFO("Physics: {0} bodies, gravity ({1}, {2})", m_Bodies.size(), gravity.x, gravity.y);
	}

	void PhysicsWorld::End()
	{
		if (!m_Running)
			return;

		// One call frees every body and shape in it, so they are not destroyed
		// individually here.
		b2DestroyWorld(g_World);
		g_World = b2_nullWorldId;

		m_Bodies.clear();
		m_Running = false;
	}

	void PhysicsWorld::AddBody(Entity& e)
	{
		if (!m_Running || e.Body == Entity::BodyType::None || e.ID == 0)
			return;
		if (m_Bodies.count(e.ID))
			return;

		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.type = ToBox2D(e.Body);
		bodyDef.position = { e.Position.x, e.Position.y };
		bodyDef.rotation = b2MakeRot(glm::radians(e.Rotation));
		bodyDef.fixedRotation = e.FixedRotation;
		bodyDef.userData = (void*)(uintptr_t)e.ID;

		b2BodyId body = b2CreateBody(g_World, &bodyDef);

		// The quad spans 0.75 either side of its origin in model space, so the
		// collider matches the sprite when ColliderSize is 1.
		const float halfX = std::max(0.001f, 0.75f * e.Scale.x * e.ColliderSize.x);
		const float halfY = std::max(0.001f, 0.75f * e.Scale.y * e.ColliderSize.y);
		b2Polygon box = b2MakeBox(halfX, halfY);

		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.density = std::max(0.0f, e.Density);
		shapeDef.material.friction = e.Friction;
		shapeDef.material.restitution = e.Restitution;
		shapeDef.isSensor = e.IsSensor;

		// Off by default in v3: without these the step reports nothing and
		// OnCollision would never fire.
		shapeDef.enableContactEvents = true;
		shapeDef.enableSensorEvents = true;

		b2CreatePolygonShape(body, &shapeDef, &box);

		BodyHandle handle{};
		std::memcpy(&handle, &body, sizeof(body));
		m_Bodies[e.ID] = handle;
	}

	void PhysicsWorld::DestroyBody(uint32_t entityID)
	{
		if (!m_Running)
			return;

		auto found = m_Bodies.find(entityID);
		if (found == m_Bodies.end())
			return;

		b2BodyId body{};
		std::memcpy(&body, &found->second, sizeof(body));
		b2DestroyBody(body);
		m_Bodies.erase(found);
	}

	void PhysicsWorld::Step(float deltaTime, std::vector<Entity>& entities, std::vector<Contact>& outContacts)
	{
		if (!m_Running)
			return;

		// A fixed step, so the simulation behaves the same regardless of frame
		// rate. The accumulator is clamped because a long stall would otherwise
		// try to catch up in one frame and lock the program up.
		constexpr float step = 1.0f / 60.0f;
		constexpr int subSteps = 4;
		constexpr int maxStepsPerFrame = 5;

		m_Accumulator += deltaTime;
		if (m_Accumulator > step * maxStepsPerFrame)
			m_Accumulator = step * maxStepsPerFrame;

		int stepped = 0;
		while (m_Accumulator >= step)
		{
			b2World_Step(g_World, step, subSteps);
			m_Accumulator -= step;
			stepped++;

			// Drained every step: events are per-step and would otherwise be
			// overwritten before anyone read them.
			b2ContactEvents contacts = b2World_GetContactEvents(g_World);
			for (int i = 0; i < contacts.beginCount; i++)
				outContacts.push_back({ EntityIdOf(contacts.beginEvents[i].shapeIdA),
				                        EntityIdOf(contacts.beginEvents[i].shapeIdB), true });
			for (int i = 0; i < contacts.endCount; i++)
				outContacts.push_back({ EntityIdOf(contacts.endEvents[i].shapeIdA),
				                        EntityIdOf(contacts.endEvents[i].shapeIdB), false });

			b2SensorEvents sensors = b2World_GetSensorEvents(g_World);
			for (int i = 0; i < sensors.beginCount; i++)
				outContacts.push_back({ EntityIdOf(sensors.beginEvents[i].sensorShapeId),
				                        EntityIdOf(sensors.beginEvents[i].visitorShapeId), true });
			for (int i = 0; i < sensors.endCount; i++)
				outContacts.push_back({ EntityIdOf(sensors.endEvents[i].sensorShapeId),
				                        EntityIdOf(sensors.endEvents[i].visitorShapeId), false });
		}

		if (stepped == 0)
			return;

		// Simulation is the authority on where a body is while playing, so its
		// transform is copied back onto the entity the renderer reads.
		for (Entity& e : entities)
		{
			auto found = m_Bodies.find(e.ID);
			if (found == m_Bodies.end())
				continue;

			b2BodyId body{};
			std::memcpy(&body, &found->second, sizeof(body));

			b2Vec2 position = b2Body_GetPosition(body);
			e.Position.x = position.x;
			e.Position.y = position.y;
			e.Rotation = glm::degrees(b2Rot_GetAngle(b2Body_GetRotation(body)));
		}
	}

}
