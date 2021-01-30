#include <Windows.h>
#include "Application.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	AppConfig config;
	Application* app = new Application(config);
	app->Run();
	delete app;
}