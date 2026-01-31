#pragma once

#ifdef CROWN_PLATFORM_WINDOWS

extern Crown::Application* Crown::CreateApplication();

int main(int argc, char** argv)
{
	Crown::Log::Init();
	CROWN_CORE_WARN("Crown Engine Initialized.");
	int a = 5;
	CROWN_INFO("Welcome to Crown Engine!");

	auto app = Crown::CreateApplication();
	app->Run();
	delete app;
}
#endif