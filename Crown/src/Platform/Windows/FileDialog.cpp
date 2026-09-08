#include "crpch.h"
#include "FileDialog.h"

#include <commdlg.h>

namespace Crown {

	namespace {

		// Scenes and textures are stored relative to the asset root so a project
		// still opens on another machine. The dialog hands back an absolute path,
		// so bring it back inside the root when it belongs there.
		std::string MakeRelativeToAssetRoot(const std::wstring& absolute)
		{
			namespace fs = std::filesystem;
			std::error_code ec;

			fs::path chosen(absolute);
			fs::path relative = fs::relative(chosen, fs::current_path(ec), ec);

			// Outside the asset root, relative() climbs out with "..". Keep the
			// absolute path in that case: wrong but honest, and the log will say
			// so if it fails to load.
			if (ec || relative.empty() || relative.native().rfind(L"..", 0) == 0)
				return chosen.string();

			// Forward slashes, so a path written on Windows reads the same
			// everywhere and matches what the engine already stores.
			std::string result = relative.generic_string();
			return result;
		}

		std::wstring RunDialog(const char* filter, const char* defaultExtension, bool saving)
		{
			wchar_t file[MAX_PATH] = L"";

			// The filter is a double-null-terminated list, so it cannot go through
			// the usual narrow-to-wide helpers.
			std::wstring wideFilter;
			for (const char* p = filter; ; p++)
			{
				wideFilter.push_back((wchar_t)*p);
				if (*p == '\0' && *(p + 1) == '\0')
					break;
			}
			wideFilter.push_back(L'\0');

			std::wstring wideExtension;
			if (defaultExtension)
				wideExtension.assign(defaultExtension, defaultExtension + std::strlen(defaultExtension));

			OPENFILENAMEW ofn{};
			ofn.lStructSize = sizeof(ofn);
			ofn.lpstrFile = file;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = wideFilter.c_str();
			ofn.nFilterIndex = 1;
			ofn.lpstrDefExt = wideExtension.empty() ? nullptr : wideExtension.c_str();

			// OFN_NOCHANGEDIR is the important one: without it the dialog leaves
			// the process in whatever folder the user browsed to, and every
			// relative asset path stops resolving from that point on.
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
			if (!saving)
				ofn.Flags |= OFN_FILEMUSTEXIST;
			else
				ofn.Flags |= OFN_OVERWRITEPROMPT;

			BOOL chosen = saving ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
			return chosen ? std::wstring(file) : std::wstring();
		}

	}

	std::string FileDialog::Open(const char* filter)
	{
		std::wstring chosen = RunDialog(filter, nullptr, false);
		return chosen.empty() ? std::string() : MakeRelativeToAssetRoot(chosen);
	}

	std::string FileDialog::Save(const char* filter, const char* defaultExtension)
	{
		std::wstring chosen = RunDialog(filter, defaultExtension, true);
		return chosen.empty() ? std::string() : MakeRelativeToAssetRoot(chosen);
	}

}
