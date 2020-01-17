class grad_meh_loading
{
	idd = -1;
	movingEnable = false;
	onLoad = "uiNamespace setVariable ['grad_meh_loadingDisplay', (_this select 0)];";
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
			idc = IDC_LOADINGLIST;
			x = safezoneX + SPACING * GRID_W;
			y = safezoneY + SPACING * GRID_H;
			w = safezoneW - SPACING * 2 * GRID_W;
			h = safezoneH - SPACING * 2 * GRID_H;
			class Controls {};
		};
	}
};