#pragma once

#ifdef CROWN_PLATFORM_WINDOWS

extern Crown::Application* Crown::CreateApplication();

int main(int argc, char** argv)
{
	auto app = Crown::CreateApplication();
	app->Run();
	delete app;
}
#endif