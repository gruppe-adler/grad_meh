#define DIALOG_WIDTH (60 * GRID_W)
#define DIALOG_HEIGHT (80 * GRID_H)
#define DIALOG_TITLE "Gruppe Adler MEH"
#define DIALOG_ONLY_CLOSE true

class grad_meh_loading {
	idd = -1;
	movingEnable = false;
	onLoad = "uiNamespace setVariable ['grad_meh_loadingDisplay', (_this select 0)];";
	onUnLoad = "_this call (uiNamespace getVariable 'grad_meh_fnc_loading_onUnLoad');";
	#include "base\start.hpp"
	#include "base\end.hpp"
};

#undef DIALOG_WIDTH
#undef DIALOG_HEIGHT
#undef DIALOG_TITLE
#undef DIALOG_ONLY_CLOSE