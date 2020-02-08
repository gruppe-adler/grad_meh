// Usage:
// #define DIALOG_WIDTH (100 * GRID_W)
// #define DIALOG_HEIGHT (80 * GRID_H)
// #define DIALOG_TITLE "grad_meh"
// #define DIALOG_ONLY_CLOSE true
// #define DIALOG_NON_SCROLLABLE true
// #include "\path\to\start.hpp"

#define DIALOG_W DIALOG_WIDTH
#define DIALOG_H DIALOG_HEIGHT
#define DIALOG_X CENTER_X(DIALOG_W)
#define DIALOG_Y CENTER_Y(DIALOG_H)

#define DIALOG_FOOTER_H (3.5 * GRID_H)
#define DIALOG_TITLE_H (2.5 * GRID_H)
#define DIALOG_BUTTON_SPACING 0.5

class ControlsBackground {
	class background: ctrlStatic {
		colorBackground[] = {0.5, 0.5, 0.5,1};
		x = safezoneX;
		y = safezoneY;
		w = safezoneW;
		h = safezoneH;
	};

	class dialog_background: ctrlStaticBackground {
		x = QUOTE(DIALOG_X);
		y = QUOTE(DIALOG_Y);
		w = QUOTE(DIALOG_W);
		h = QUOTE(DIALOG_H + DIALOG_FOOTER_H);
	};
	class dialog_title: ctrlStaticTitle {
		moving = 0;
		x = QUOTE(DIALOG_X);
		y = QUOTE(DIALOG_Y - DIALOG_TITLE_H);
		w = QUOTE(DIALOG_W);
		h = QUOTE(DIALOG_TITLE_H);
		text = DIALOG_TITLE;
	};
	class dialog_footer: ctrlStaticFooter {
		x = QUOTE(DIALOG_X);
		y = QUOTE(DIALOG_Y + DIALOG_H);
		w = QUOTE(DIALOG_W);
		h = QUOTE(DIALOG_FOOTER_H);
	};
#ifndef DIALOG_ONLY_CLOSE
	class dialog_btnOk: ctrlButtonOK
	{
		x = QUOTE(DIALOG_X + DIALOG_W - 29 * GRID_W);
		y = QUOTE(DIALOG_Y + DIALOG_H + DIALOG_BUTTON_SPACING * GRID_H);
		w = QUOTE(14 * GRID_W);
		h = QUOTE(DIALOG_FOOTER_H - DIALOG_BUTTON_SPACING * 2 * GRID_H);
	};
	class dialog_btnCancel: ctrlButtonCancel
#else
	class dialog_btnClose: ctrlButtonClose
#endif
	{
		x = QUOTE(DIALOG_X + DIALOG_W - 14.5 * GRID_W);
		y = QUOTE(DIALOG_Y + DIALOG_H + DIALOG_BUTTON_SPACING * GRID_H);
		w = QUOTE(14 * GRID_W);
		h = QUOTE(DIALOG_FOOTER_H - DIALOG_BUTTON_SPACING * 2 * GRID_H);
	};
};
class Controls {
#ifdef DIALOG_NON_SCROLLABLE
	class content: ctrlControlsGroupNoScrollbars {
#else
	class content: ctrlControlsGroupNoHScrollbars {
#endif
		idc = IDC_DIALOG_CONTENT;
		x = QUOTE(DIALOG_X);
		y = QUOTE(DIALOG_Y);
		w = QUOTE(DIALOG_W);
		h = QUOTE(DIALOG_H);
		class Controls {


#undef DIALOG_W
#undef DIALOG_H
#undef DIALOG_X
#undef DIALOG_Y
#undef DIALOG_FOOTER_H
#undef DIALOG_TITLE_H
#undef DIALOG_BUTTON_SPACING