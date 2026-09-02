#pragma once

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

		int AtlasCell = 0;                        // index into the sprite atlas
		glm::vec4 Tint{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

}
