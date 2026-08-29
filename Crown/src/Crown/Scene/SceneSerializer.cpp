#include "crpch.h"
#include "SceneSerializer.h"

#include <fstream>

namespace Crown {

	static constexpr const char* s_Magic = "crown-scene";
	static constexpr int s_Version = 1;

	bool SaveScene(const std::vector<Entity>& entities, const std::string& path)
	{
		namespace fs = std::filesystem;
		std::error_code ec;

		fs::path target(path);
		if (target.has_parent_path())
			fs::create_directories(target.parent_path(), ec);

		// Write beside the target and rename over it, so a crash midway leaves
		// the previous scene intact rather than a half-written file.
		fs::path temp = target; temp += ".tmp";
		{
			std::ofstream out(temp);
			if (!out)
			{
				CROWN_CORE_ERROR("SaveScene: cannot open '{0}' for writing", temp.string());
				return false;
			}

			out << s_Magic << ' ' << s_Version << '\n';
			for (const Entity& e : entities)
			{
				out << "entity\n";
				out << "name "  << e.Name << '\n';                                    // rest of line
				out << "pos "   << e.Position.x << ' ' << e.Position.y << ' ' << e.Position.z << '\n';
				out << "rot "   << e.Rotation << '\n';
				out << "scale " << e.Scale.x << ' ' << e.Scale.y << '\n';
				out << "cell "  << e.AtlasCell << '\n';
				out << "tint "  << e.Tint.r << ' ' << e.Tint.g << ' ' << e.Tint.b << ' ' << e.Tint.a << '\n';
			}
			if (!out)
			{
				CROWN_CORE_ERROR("SaveScene: write failed for '{0}'", temp.string());
				return false;
			}
		}

		fs::rename(temp, target, ec);
		if (ec)
		{
			CROWN_CORE_ERROR("SaveScene: could not move '{0}' into place ({1})", temp.string(), ec.message());
			fs::remove(temp, ec);
			return false;
		}

		CROWN_CORE_INFO("Saved {0} entities to '{1}'", entities.size(), path);
		return true;
	}

	bool LoadScene(std::vector<Entity>& entities, const std::string& path)
	{
		std::ifstream in(path);
		if (!in)
		{
			CROWN_CORE_ERROR("LoadScene: cannot open '{0}'", path);
			return false;
		}

		std::string line, magic;
		int version = 0;
		if (!std::getline(in, line) || !(std::istringstream(line) >> magic >> version) || magic != s_Magic)
		{
			CROWN_CORE_ERROR("LoadScene: '{0}' is not a Crown scene", path);
			return false;
		}
		if (version > s_Version)
		{
			CROWN_CORE_ERROR("LoadScene: '{0}' is version {1}, this build understands {2}", path, version, s_Version);
			return false;
		}

		// Parse into a temporary; the caller's scene is only replaced on success.
		std::vector<Entity> parsed;
		int lineNumber = 1;
		while (std::getline(in, line))
		{
			lineNumber++;
			std::istringstream ls(line);
			std::string key;
			if (!(ls >> key) || key.empty())
				continue;                                  // blank line

			if (key == "entity") { parsed.emplace_back(); continue; }

			if (parsed.empty())
			{
				CROWN_CORE_ERROR("LoadScene: '{0}' line {1}: '{2}' before any entity", path, lineNumber, key);
				return false;
			}

			Entity& e = parsed.back();
			bool ok = true;
			if      (key == "name")  { std::getline(ls >> std::ws, e.Name); }
			else if (key == "pos")   { ok = (bool)(ls >> e.Position.x >> e.Position.y >> e.Position.z); }
			else if (key == "rot")   { ok = (bool)(ls >> e.Rotation); }
			else if (key == "scale") { ok = (bool)(ls >> e.Scale.x >> e.Scale.y); }
			else if (key == "cell")  { ok = (bool)(ls >> e.AtlasCell); }
			else if (key == "tint")  { ok = (bool)(ls >> e.Tint.r >> e.Tint.g >> e.Tint.b >> e.Tint.a); }
			else continue;                                 // unknown key: ignore, so older builds tolerate newer files

			if (!ok)
			{
				CROWN_CORE_ERROR("LoadScene: '{0}' line {1}: malformed '{2}'", path, lineNumber, key);
				return false;
			}
		}

		entities = std::move(parsed);
		CROWN_CORE_INFO("Loaded {0} entities from '{1}'", entities.size(), path);
		return true;
	}

}
