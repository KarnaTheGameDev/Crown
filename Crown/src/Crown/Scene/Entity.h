#pragma once

#include <string>
#include <glm/glm.hpp>

namespace Crown {

	// A plain struct, not an ECS. With a few hundred sprites a vector of these
	// is faster to read and to write than a component store, and swapping in
	// entt later only touches the code that iterates them.
	struct Entity
	{
		std::string Name = "Entity";

		glm::vec3 Position{ 0.0f, 0.0f, 0.0f };
		float Rotation = 0.0f;                    // degrees, about Z
		glm::vec2 Scale{ 1.0f, 1.0f };

		int AtlasCell = 0;                        // index into the sprite atlas
		glm::vec4 Tint{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

}
