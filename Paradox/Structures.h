#pragma once
#include <string>

struct Config
{
	Config(const std::string windowTitle = "Paradox",
		unsigned int width = 1280,
		unsigned int height = 960)
		:
		windowTitle(windowTitle),
		width(width),
		height(height)
	{}

	std::string windowTitle;
	unsigned int width, height;
};