#define DIALOG_WIDTH (80 * GRID_W)
#define DIALOG_HEIGHT (57 * GRID_H)
#define DIALOG_TITLE "Gruppe Adler MEH"
#define DIALOG_ONLY_CLOSE true

class grad_meh_done {
	idd = -1;
	onUnLoad = "_this call (uiNamespace getVariable 'grad_meh_fnc_done_onUnLoad');";
	movingEnable = false;
	#include "base\start.hpp"
	class text: ctrlStructuredText {
		idc = IDC_DONE_TEXT;
		x = 0 + GRID_W;
		y = 0 + GRID_H;
		w = QUOTE(DIALOG_WIDTH - GRID_W * 2);
		h = QUOTE(DIALOG_HEIGHT - GRID_H * 2);
		text = "";
		shadow = 0;
		class Attributes: Attributes
		{
			align = "center";
			valign = "middle";
			size = 1 * GRID_H;
		};
	};
	#include "base\end.hpp"
};

#undef DIALOG_WIDTH
#undef DIALOG_HEIGHT
#undef DIALOG_TITLE
#undef DIALOG_ONLY_CLOSE
