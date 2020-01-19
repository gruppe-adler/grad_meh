#include "loadingStep.hpp"

class grad_meh_loadingItem: ctrlControlsGroupNoScrollbars {
	x = 0;
	y = 0;
	w = safezoneW;
	h = (5 + 6 * LOADING_STEP_H) * GRID_H;
	class Controls {
		class name: ctrlStatic {
			idc = IDC_LOADINGITEM_NAME;
			x = 0;
			y = 0;
			w = safezoneW;
			h = 5 * GRID_H;
			sizeEx = 5 * GRID_H
		};
		class readWrp: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_READWRP;
			y = (5 + 0 * LOADING_STEP_H) * GRID_H;
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Read WRP";
				};
			};
		};
		class satImage: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_SATIMAGE;
			y = (5 + 1 * LOADING_STEP_H) * GRID_H;
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Expot satellite image";
				};
			};
		};
		class houses: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_HOUSES;
			y = (5 + 2 * LOADING_STEP_H) * GRID_H;
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Export houses";
				};
			};
		};
		class previewImage: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_PREVIEWIMGE;
			y = (5 + 3 * LOADING_STEP_H) * GRID_H;
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Export preview image";
				};
			};
		};
		class meta: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_META;
			y = (5 + 4 * LOADING_STEP_H) * GRID_H;
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Export meta.json";
				};
			};
		};
		class dem: grad_meh_loadingStep {
			idc = IDC_LOADINGITEM_STEP_DEM;
			y = (5 + 5 * LOADING_STEP_H) * GRID_H;
			class Controls: Controls {
				class done: done {};
				class text: text {
					text = "Export digital elevation model";
				};
			};
		};
	};
};