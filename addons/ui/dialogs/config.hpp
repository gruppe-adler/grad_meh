#define ROW_HEIGHT 3
#define ROW_Y(index) ((SPACING + index * ROW_HEIGHT) * GRID_H)
#define DIALOG_WIDTH (50 * GRID_W)
#define DIALOG_HEIGHT (ROW_Y(5) + SPACING * GRID_H)
#define DIALOG_TITLE "Gruppe Adler MEH"
#define DIALOG_NON_SCROLLABLE true

class grad_meh_config {
	idd = -1;
	movingEnable = false;
	onLoad = "_this call (uiNamespace getVariable 'grad_meh_fnc_config_onLoad');";
	onUnLoad = "_this call (uiNamespace getVariable 'grad_meh_fnc_config_onUnLoad');";
	#include "base\start.hpp"
	class sat: ctrlControlsGroupNoScrollbars {
		x = SPACING * GRID_W;
		y = ROW_Y(0);
		w = DIALOG_WIDTH;
		h = ROW_HEIGHT * GRID_H;
		idc = -1;
		class Controls {
			class check: ctrlCheckbox {
				idc = IDC_CONFIG_CHECK_SAT;
				x = 0;
				y = ROW_HEIGHT * 0.05 * GRID_H;
				w = ROW_HEIGHT * 0.9 * GRID_W;
				h = ROW_HEIGHT * 0.9 * GRID_H;
				checked = 1;
			};
			class text: ctrlStatic {
				idc = -1;
				x = ROW_HEIGHT * GRID_W;
				y = 0;
				w = safezoneW - (ROW_HEIGHT) * GRID_W;
				h = ROW_HEIGHT * GRID_H;
				text = "Export satellite image";
				sizeEx = ROW_HEIGHT * 0.9 * GRID_H;
			};
		};
	};
	class houses: sat {
		y = ROW_Y(1);
		class Controls: Controls {
			class check: check {
				idc = IDC_CONFIG_CHECK_HOUSES;
			};
			class text: text {
				text = "Export houses";
			};
		};
	};
	class preview: sat {
		y = ROW_Y(2);
		class Controls: Controls {
			class check: check {
				idc = IDC_CONFIG_CHECK_PREVIEW;
			};
			class text: text {
				text = "Export preview image";
			};
		};
	};
	class meta: sat {
		y = ROW_Y(3);
		class Controls: Controls {
			class check: check {
				idc = IDC_CONFIG_CHECK_META;
			};
			class text: text {
				text = "Export meta.json";
			};
		};
	};
	class dem: sat {
		y = ROW_Y(4);
		class Controls: Controls {
			class check: check {
				idc = IDC_CONFIG_CHECK_DEM;
			};
			class text: text {
				text = "Export digital elevation model";
			};
		};
	};
	#include "base\end.hpp"
};

#undef ROW_HEIGHT
#undef ROW_Y
#undef DIALOG_WIDTH
#undef DIALOG_HEIGHT
#undef DIALOG_TITLE
#undef DIALOG_NON_SCROLLABLE