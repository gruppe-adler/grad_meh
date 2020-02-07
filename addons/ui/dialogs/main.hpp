#define DIALOG_WIDTH ((MAP_ITEM_W * 4 + 2 * SPACING) * GRID_W)
#define DIALOG_HEIGHT ((MAP_ITEM_H * 2 + 2 * SPACING) * GRID_H)
#define DIALOG_TITLE "Gruppe Adler MEH"

class grad_meh_main {
	idd = -1;
	movingEnable = false;
	onLoad = "_this call (uiNamespace getVariable 'grad_meh_fnc_main_onLoad');";
	onUnLoad = "_this call (uiNamespace getVariable 'grad_meh_fnc_main_onUnLoad');";
	#include "base\start.hpp"
	#include "base\end.hpp"
};

#undef DIALOG_WIDTH
#undef DIALOG_HEIGHT
#undef DIALOG_TITLE
