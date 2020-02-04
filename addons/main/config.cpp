#include "script_component.hpp"

class CfgPatches {
	class grad_meh_main {
		name = "Gruppe Adler Map Exporter";
		units[] = {};
		weapons[] = {};
		requiredVersion = 1.92;
		requiredAddons[] = { "intercept_core" };
		authors[] = { "Willard", "DerZade" };
		url = "";
		VERSION_CONFIG;
	};
};

class Intercept {
    class grad {
        class grad_meh {
            pluginName = "grad_meh";
        };
    };
};

#include "CfgFunctions.hpp"