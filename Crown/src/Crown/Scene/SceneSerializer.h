#pragma once

#include "Entity.h"

#include <string>
#include <vector>

namespace Crown {

	// ponytail: a flat line-oriented format, no dependency. It has no nesting
	// and no schema versioning; move to YAML or JSON if entities gain children
	// or the field set starts changing between builds.
	bool SaveScene(const std::vector<Entity>& entities, const std::string& path);

	// Only replaces `entities` if the whole file parsed. A partial load would
	// silently destroy the scene the user was editing.
	bool LoadScene(std::vector<Entity>& entities, const std::string& path);

}
