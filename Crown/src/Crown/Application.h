#pragma once

#include "Core.h"

namespace Crown {
	class CROWN_API Application
	{
	public:
		Application();
		virtual ~Application();

		// allow derived applications to override the run loop
		virtual void Run();
	};

	Application* CreateApplication();
}
