#define START_BTN_HEIGHT 8

class grad_meh_done
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
		class text: ctrlStructuredText {
			style = 2;
			x = safezoneX;
			y = safezoneY;
			w = safezoneW;
			h = safezoneH;
			text = "<img image=\"\x\grad_meh\addons\ui\data\logo_ca.paa\"/><br/>ALL DONE";
			sizeEx = 10 * GRID_H;
			class Attributes: Attributes
			{
				align="center";
				valign="middle";
				size=1;
			};
		}
	};
	class Controls {
		class close: ctrlButton {
			x = safezoneX + GRID_W * SPACING;
			y = safezoneY + safezoneH - (START_BTN_HEIGHT + SPACING) * GRID_H;
			w = safeZoneW - SPACING * 2 * GRID_W;
			h = START_BTN_HEIGHT * GRID_H;
			text = "Close";
			sizeEx = START_BTN_HEIGHT * (4/5) * GRID_H;
			onMouseButtonClick="closeDisplay (ctrlParent (_this select 0));"
		};
	};
};