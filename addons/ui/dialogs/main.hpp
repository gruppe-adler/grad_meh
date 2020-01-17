#define START_BTN_HEIGHT 8

class grad_meh_main
{
	idd = -1;
	movingEnable = false;
	onLoad = "_this call (uiNamespace getVariable 'grad_meh_fnc_onLoad');";
	onKeyDown = "_this call (uiNamespace getVariable 'grad_meh_fnc_onKeyDown');";
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
		class maplist: ctrlControlsGroupNoHScrollbars {
			idc = IDC_MAPLIST;
			x = safezoneX + SPACING * GRID_W;
			y = safezoneY + SPACING * GRID_H;
			w = safezoneW - SPACING * 2 * GRID_W;
			h = safezoneH - (SPACING * 3 + START_BTN_HEIGHT) * GRID_H;
			class Controls {};
		};
		class start: ctrlButton {
			x = safezoneX + GRID_W * SPACING;
			y = safezoneY + safezoneH - (START_BTN_HEIGHT + SPACING) * GRID_H;
			w = safeZoneW - GRID_W * SPACING * 2;
			h = START_BTN_HEIGHT * GRID_H;
			text = "Start";
			sizeEx = START_BTN_HEIGHT * (4/5) * GRID_H;
			onMouseButtonClick="_this call (uiNamespace getVariable 'grad_meh_fnc_onStartClick');"
		};
	};
};