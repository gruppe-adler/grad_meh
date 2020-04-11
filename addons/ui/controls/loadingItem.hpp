#include "loadingStep.hpp"

#define NAME_HEIGHT 5
#define BOTTOM_MARGIN 2.5
#define STEP_Y(index) ((NAME_HEIGHT + index * LOADING_STEP_H) * GRID_H)

class grad_meh_loadingItem: ctrlControlsGroupNoScrollbars {
	x = SPACING * GRID_W;
	y = SPACING * GRID_H;
	w = LOADING_STEP_W;
	h = STEP_Y(6) + BOTTOM_MARGIN * GRID_H;
	class Controls {
		class name: ctrlStatic {
			idc = IDC_LOADINGITEM_NAME;
			x = 0;
			y = 0;
			w = LOADING_STEP_W;
			h = NAME_HEIGHT * GRID_H;
			sizeEx = NAME_HEIGHT * GRID_H
		};
		class readWrp: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_READWRP;
			y = STEP_Y(0);
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Read WRP";
				};
			};
		};
		class satImage: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_SATIMAGE;
			y = STEP_Y(1);
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Export satellite image";
				};
			};
		};
		class houses: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_HOUSES;
			y = STEP_Y(2);
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Export geojsons";
				};
			};
		};
		class previewImage: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_PREVIEWIMGE;
			y = STEP_Y(3);
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Export preview image";
				};
			};
		};
		class meta: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_META;
			y = STEP_Y(4);
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Export meta.json";
				};
			};
		};
		class dem: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_DEM;
			y = STEP_Y(5);
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Export digital elevation model";
				};
			};
		};
	};
};

class grad_meh_loadingItem_error: grad_meh_loadingItem {
	h = STEP_Y(1) + BOTTOM_MARGIN * GRID_H;
	class Controls {
		class name: ctrlStatic {
			idc = IDC_LOADINGITEM_NAME;
			x = 0;
			y = 0;
			w = LOADING_STEP_W;
			h = NAME_HEIGHT * GRID_H;
			sizeEx = NAME_HEIGHT * GRID_H
		};
		class error: name {
			idc = IDC_LOADINGITEM_ERROR;
			y = STEP_Y(0);
			h = LOADING_STEP_H * GRID_H;
			sizeEx = LOADING_STEP_H * 0.9 * GRID_H;
		};
	};
};

#undef NAME_HEIGHT
#undef BOTTOM_MARGIN
#undef STEP_Y