#include <Windows.h>
#include "Application.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	Config config;
	Application* app = (Application*)malloc(sizeof(Application));
	app = new Application(config);
	app->Run();
	delete app;
}