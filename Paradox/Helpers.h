#pragma once
#include <Windows.h>

namespace Helpers
{
	void Validate(HRESULT hr, LPWSTR message)
	{
		if (FAILED(hr))
		{
			MessageBox(NULL, message, L"Error", MB_OK);
			PostQuitMessage(EXIT_FAILURE);
		}
	}
};