
class grad_meh_mapItem: ctrlControlsGroupNoScrollbars {
	x = 0;
	y = 0;
	w = MAP_ITEM_W * GRID_W;
	h = MAP_ITEM_H * GRID_H;
	class Controls {
		class background: ctrlStatic {
			idc = IDC_MAPITEM_BACKGROUND;
			colorBackground[] = {1,1,1,1};
			x = 0;
			y = 0;
			w = MAP_ITEM_W * GRID_W;
			h = MAP_ITEM_H * GRID_H;
			show = 0;
		};
		class picture: ctrlStaticPictureKeepAspect {
			idc = IDC_MAPITEM_PICTURE;
			x = SPACING / 2 * GRID_W;
			y = SPACING / 2 * GRID_W;
			w = (MAP_ITEM_W - SPACING) * GRID_W;
			h = (MAP_ITEM_W - SPACING) * GRID_H;
		};
		class name: ctrlStatic {
			idc = IDC_MAPITEM_NAME;
			x = SPACING / 2 * GRID_W;
			y = (MAP_ITEM_W - SPACING + SPACING * 1.5) * GRID_H;
			w = (MAP_ITEM_W - SPACING) * GRID_W;
			h = 5 * GRID_H;
			sizeEx = 5 * GRID_H
		};
		class author: ctrlStatic {
			idc = IDC_MAPITEM_AUTHOR;
			x = SPACING / 2 * GRID_W;
			y = (MAP_ITEM_W - SPACING + SPACING * 1.5 + 5) * GRID_H;
			w = (MAP_ITEM_W - SPACING) * GRID_W;
			h = 3 * GRID_H;
			sizeEx = 3 * GRID_H;
			colorText[] = {0.7, 0.7, 0.7, 1};
		};
		class selectIndicator: ctrlStaticPictureKeepAspect {
			idc = IDC_MAPITEM_SELECTINDICATOR;
			x = MAP_ITEM_W / 3 * GRID_W;
			y = MAP_ITEM_H / 3 * GRID_W;
			w = MAP_ITEM_W * GRID_W / 3;
			h = MAP_ITEM_W * GRID_H / 3;
			text = "\x\grad_meh\addons\ui\data\tick_ca.paa";
			show = 0;
		};
		class mouseHandler: ctrlButton {
			idc = -1;
			x = 0;
			y = 0;
			w = MAP_ITEM_W * GRID_W;
			h = MAP_ITEM_H * GRID_H;
			colorBackground[]={0,0,0,0};
			colorBackgroundActive[]={0,0,0,0};
			onMouseEnter=QUOTE(params ['_c']; ((ctrlParentControlsGroup _c) controlsGroupCtrl IDC_MAPITEM_BACKGROUND) ctrlShow true;);
			onMouseExit=QUOTE(params ['_c']; ((ctrlParentControlsGroup _c) controlsGroupCtrl IDC_MAPITEM_BACKGROUND) ctrlShow false;);
			onMouseButtonClick="_this call (uiNamespace getVariable 'grad_meh_fnc_mapItem_onClick');";
		};
	};
};