#pragma once

#include "Events/ApplicationEvent.h"
#include "Window.h"
#include "Physics/PhysicsApp.h"

class Application
{
public:
	Application(Config config);
	virtual ~Application();

	int Run();

	void OnEvent(Event& event);

	inline static Application& Get() { return *s_Instance; }

	inline Window& GetWindow() { return *m_Window; }

	inline RigidBodyApplication& GetRigidBodyApp() { return *m_RigidBodyApp; }


private:
	bool m_Running = true;
	bool OnWindowClose(WindowCloseEvent& event);

	std::unique_ptr<Window> m_Window;
	static Application* s_Instance;
	std::unique_ptr<RigidBodyApplication> m_RigidBodyApp;
};