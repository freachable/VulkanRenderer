#include "Application.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

int main()
{
	if (Application::enableValidationLayers && std::getenv("VK_ADD_LAYER_PATH") == nullptr)
	{
#ifdef _WIN32
		_putenv_s("VK_ADD_LAYER_PATH", VR_VALIDATION_LAYER_DIR);
#else
		setenv("VK_ADD_LAYER_PATH", VR_VALIDATION_LAYER_DIR, 0);
#endif
	}

	try
	{
		Application app;
		app.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
