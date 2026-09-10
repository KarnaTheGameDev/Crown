#pragma once

#include <any>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace Crown {

	// A plain struct, not an ECS. With a few hundred sprites a vector of these
	// is faster to read and to write than a component store, and swapping in
	// entt later only touches the code that iterates them.
	struct Entity
	{
		// Stable for the entity's lifetime, including across save and load.
		// Hold one of these rather than an index: the vector shifts whenever
		// anything is deleted, so an index silently starts referring to a
		// different entity. 0 means unassigned.
		uint32_t ID = 0;

		std::string Name = "Entity";

		glm::vec3 Position{ 0.0f, 0.0f, 0.0f };
		float Rotation = 0.0f;                    // degrees, about Z
		glm::vec2 Scale{ 1.0f, 1.0f };

		// Path to a texture, relative to the asset root. Empty draws plain
		// colour, so an entity is visible before any art is chosen.
		std::string Texture;

		// Which part of that texture to draw: xy is the origin, zw the size,
		// both in UV space. The whole image by default; a sub-rectangle picks
		// one cell out of a sprite sheet.
		glm::vec4 SpriteRect{ 0.0f, 0.0f, 1.0f, 1.0f };
		glm::vec4 Tint{ 1.0f, 1.0f, 1.0f, 1.0f };

		// --- physics ---------------------------------------------------------
		// A description, not the simulation itself. Box2D owns the running
		// state while playing and writes Position and Rotation back each step;
		// these fields say what to build when play starts.

		// None leaves the entity out of the simulation entirely, which is the
		// default so that adding physics is a deliberate act.
		enum class BodyType { None, Static, Dynamic, Kinematic };
		BodyType Body = BodyType::None;

		// Multiplies Scale, so a collider tracks the sprite unless told not to.
		glm::vec2 ColliderSize{ 1.0f, 1.0f };

		float Density = 1.0f;
		float Friction = 0.3f;
		float Restitution = 0.0f;      // bounciness, 0 to 1

		// Reports overlaps without pushing anything: pickups, triggers, goals.
		bool IsSensor = false;

		// Stops a body tipping over. What most 2D characters want.
		bool FixedRotation = false;

		// Whatever the game needs to hang off this entity: velocity, health, a
		// state machine. The engine never reads it, so it stays out of the
		// engine's vocabulary, and it dies with the entity rather than leaving
		// the client to clean up a parallel container.
		//
		// Deliberately not serialised. Gameplay state is transient; a saved
		// scene is the arrangement of things, not the middle of a game.
		//
		//   e.UserData = Mover{ velocity, spin };
		//   if (Mover* m = std::any_cast<Mover>(&e.UserData)) ...
		std::any UserData;
	};

}
