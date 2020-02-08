class grad_meh_loadingStep: ctrlControlsGroupNoScrollbars {
	x = 0;
	y = (5 + 0 * LOADING_STEP_H) * GRID_H;
	w = LOADING_STEP_W * GRID_W;
	h = LOADING_STEP_H * GRID_H;
	idc = -1;
	deletable = 1;
	class Controls {
		class done: ctrlStaticPictureKeepAspect {
			idc = IDC_LOADINGSTEP_PICTURE;
			x = 0;
			y = 0;
			w = LOADING_STEP_H * 0.9 * GRID_W;
			h = LOADING_STEP_H * 0.9 * GRID_H;
			text = "\a3\3den\data\controls\ctrlcheckbox\textureunchecked_ca.paa";
		};
		class text: ctrlStatic {
			idc = IDC_LOADINGSTEP_TEXT;
			x = (LOADING_STEP_H) * GRID_W;
			y = 0;
			w = safezoneW - (LOADING_STEP_H) * GRID_W;
			h = LOADING_STEP_H * GRID_H;
			text = "";
			sizeEx = LOADING_STEP_H * 0.9 * GRID_H;
		};
	};
};