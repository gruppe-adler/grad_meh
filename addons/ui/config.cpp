#include "script_component.hpp"

class CfgPatches {
	class grad_meh_ui {
		name = "Gruppe Adler Map Exporter - UI";
		units[] = {};
		weapons[] = {};
		requiredVersion = 1.92;
		requiredAddons[] = { "grad_meh_main" };
		authors[] = { "Willard", "DerZade" };
		url = "";
		VERSION_CONFIG;
	};
};

class ctrlControlsGroupNoScrollbars;
class ctrlStatic;
class ctrlStaticPictureKeepAspect;
class ctrlControlsGroupNoHScrollbars;
class ctrlButton;
class ctrlCheckbox;
class ctrlButtonSearch;
class ctrlStructuredText;
class ctrlStaticBackground;
class ctrlStaticTitle;
class ctrlStaticFooter;
class ctrlButtonOK;
class ctrlButtonClose;
class ctrlButtonCancel;

#include "idcmacros.hpp"

// controls
#include "controls\loadingItem.hpp"
#include "controls\mapItem.hpp"

// dialogs
#include "dialogs\main.hpp"
#include "dialogs\loading.hpp"
#include "dialogs\config.hpp"
#include "dialogs\done.hpp"

#include "CfgFunctions.hpp"

class RscStandardDisplay;
class RscDisplayMain: RscStandardDisplay
{
	class ControlsBackground 
	{
		class grad_onLoadHandler: ctrlStatic {
			x = 0;
			y = 0;
			w = 0;
			h = 0;
			onLoad="params ['_ctrl']; (ctrlParent _ctrl) createDisplay 'grad_meh_main';";
		};
	};
};