#pragma once

#include <string>

namespace Crown {

	// Native open/save dialogs. Both return an empty string if the user cancels.
	//
	// Paths come back relative to the asset root when the chosen file lives
	// under it, because that is what scenes and textures store: a scene saved on
	// one machine has to load on another.
	namespace FileDialog {

		// filter is a Windows double-null-terminated pair list, for example
		// "Images\0*.png;*.jpg\0All\0*.*\0".
		std::string Open(const char* filter);
		std::string Save(const char* filter, const char* defaultExtension);

	}

}
