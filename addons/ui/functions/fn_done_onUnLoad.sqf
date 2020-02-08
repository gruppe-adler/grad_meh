#include "../idcmacros.hpp"

params ["_display", "_exitCode"];

// exit mission
[] spawn {
	sleep 0.5;
	endMission "END1";
};