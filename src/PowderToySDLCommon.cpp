#include "PowderToySDL.h"
#include "gui/interface/Engine.h"

void MainLoop()
{
	TptDiagLog("MainLoop entered, Running=%d", int(ui::Engine::Ref().Running()));
	int iterations = 0;
	while (ui::Engine::Ref().Running())
	{
		auto delay = EngineProcess();
		if (iterations < 5)
		{
			TptDiagLog("MainLoop iter=%d delay=%lld", iterations,
				delay.has_value() ? (long long)*delay : -1LL);
		}
		iterations += 1;
		if (delay.has_value())
		{
			SDL_Delay(std::max(*delay, UINT64_C(1)));
		}
	}
	TptDiagLog("MainLoop exited after %d iterations", iterations);
}

void ApplyFpsLimit()
{
}
