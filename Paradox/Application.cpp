#include <assert.h>
#include "Application.h"

#define BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)

Application* Application::s_Instance = nullptr;

Application::Application(AppConfig config)
{
	assert(!s_Instance);
	s_Instance = this;
	WindowProps props(config.windowTitle, config.width, config.height);
	m_Window = std::unique_ptr<Window>(new Window(props));
	m_Window->SetEventCallback(BIND_EVENT_FN(Application::OnEvent));
};

Application::~Application()
{

}

void Application::OnEvent(Event& event)
{
	EventDispatcher dispatcher(event);
	dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));

}

int Application::Run()
{
	while (m_Running)
	{
		if (const auto ecode = Window::ProcessMessages())
		{
			return *ecode;
		}
	}

	return 0;
}

bool Application::OnWindowClose(WindowCloseEvent& event)
{
	m_Running = false;
	return true;
}