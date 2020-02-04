class grad_meh_config
{
	idd = -1;
	movingEnable = false;
	class ControlsBackground
	{
		class main: ctrlStatic {
			colorBackground[] = {0.231,0.231,0.231,1};
			x = safezoneX;
			y = safezoneY;
			w = safezoneW;
			h = safezoneH;
		};
		class logo: ctrlStaticPictureKeepAspect {
			x = safezoneX;
			y = safezoneY;
			w = safezoneW;
			h = safezoneH;
			text="\x\grad_meh\addons\ui\data\logo_ca.paa";
			colorText[] = {1,1,1,0.3};
		}
	};
	class Controls {
		class sat: ctrlControlsGroupNoScrollbars {
			x = 4 * GRID_W;
			y = (5 + 0 * LOADING_STEP_H) * GRID_H;
			w = safezoneW;
			h = LOADING_STEP_H * GRID_H;
			idc = -1;
			class Controls {
				class check: ctrlCheckbox {
					idc = IDC_CONFIG_CHECK_SAT;
					x = 0;
					y = LOADING_STEP_H * 0.05 * GRID_H;
					w = LOADING_STEP_H * 0.9 * GRID_W;
					h = LOADING_STEP_H * 0.9 * GRID_H;
				};
				class text: ctrlStatic {
					idc = -1;
					x = (LOADING_STEP_H) * GRID_W;
					y = 0;
					w = safezoneW - (LOADING_STEP_H) * GRID_W;
					h = LOADING_STEP_H * GRID_H;
					text = "Expot satellite image";
					sizeEx = LOADING_STEP_H * 0.9 * GRID_H;
				};
			};
		};
		class houses: sat {
			y = (5 + 1 * LOADING_STEP_H) * GRID_H;
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
			y = (5 + 2 * LOADING_STEP_H) * GRID_H;
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
			y = (5 + 3 * LOADING_STEP_H) * GRID_H;
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
			y = (5 + 4 * LOADING_STEP_H) * GRID_H;
			class Controls: Controls {
				class check: check {
					idc = IDC_CONFIG_CHECK_DEM;
				};
				class text: text {
					text = "Export digital elevation model";
				};
			};
		};
		class start: ctrlButton {
			x = safezoneX + GRID_W * SPACING;
			y = safezoneY + safezoneH - (START_BTN_HEIGHT + SPACING) * GRID_H;
			w = safeZoneW - SPACING * 2 * GRID_W;
			h = START_BTN_HEIGHT * GRID_H;
			text = "Start";
			sizeEx = START_BTN_HEIGHT * (4/5) * GRID_H;
			onMouseButtonClick="_this call (uiNamespace getVariable 'grad_meh_fnc_onConfigStartClick');"
		};
	};
};