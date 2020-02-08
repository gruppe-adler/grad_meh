#define DIALOG_WIDTH (80 * GRID_W)
#define DIALOG_HEIGHT (57 * GRID_H)
#define DIALOG_TITLE "Gruppe Adler MEH"
#define DIALOG_ONLY_CLOSE true
#define DIALOG_NON_SCROLLABLE true

class grad_meh_done {
	idd = -1;
	onUnLoad = "_this call (uiNamespace getVariable 'grad_meh_fnc_done_onUnLoad');";
	movingEnable = false;
	#include "base\start.hpp"
	class text: ctrlStructuredText {
		x = 0 + GRID_W;
		y = 0 + GRID_H;
		w = QUOTE(DIALOG_WIDTH - GRID_W * 2);
		h = QUOTE(DIALOG_HEIGHT - GRID_H * 2);
		text = QUOTE(<t size='20 * GRID_H'><img image='\x\grad_meh\addons\ui\data\logo_ca.paa'/></t><br/>EXPORT FINISHED);
		shadow = 0;
		class Attributes: Attributes
		{
			align = "center";
			valign = "middle";
			size = 5 * GRID_H;
		};
	};
	#include "base\end.hpp"
};

#undef DIALOG_WIDTH
#undef DIALOG_HEIGHT
#undef DIALOG_TITLE
#undef DIALOG_ONLY_CLOSE
#undef DIALOG_NON_SCROLLABLE
