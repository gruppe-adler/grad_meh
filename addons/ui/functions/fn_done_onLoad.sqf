#include "../idcmacros.hpp"
#define LOGS_ARROW "<img image='\a3\ui_f\data\gui\rsc\rscdisplaymultiplayer\arrow_down_ca.paa' />"

params ["_display"];

private _parentGrp = _display displayCtrl IDC_DIALOG_CONTENT;
private _errors = uiNamespace getVariable ["grad_meh_errors", []];
private _progress = uiNamespace getVariable ["grad_meh_progress", []];
private _canceledCount = 0;
{
	(_x select 1) params ["_running", "_done", "_canceled"];
	_canceledCount = _canceledCount + count _canceled;
} forEach _progress;
private _totalErrors = count _errors + _canceledCount;
private _yPos = 0;

private _textCtrl = (_display displayCtrl IDC_DONE_TEXT);
if !(isNull _textCtrl) then {
	// update yPos to make sure logs start below the text
	private _textPos = ctrlPosition (_display displayCtrl IDC_DONE_TEXT);
	_yPos = (SPACING * GRID_H) + (_textPos select 1) + (_textPos select 3);

	// update text
	private _errorColor = ["#FFFFFF", "#8F1167"] select (_totalErrors > 0);

	_textCtrl ctrlSetStructuredText parseText format [
		"
<t size='15'><img image='\x\grad_meh\addons\ui\data\logo_ca.paa'/></t><br />
<t size='5'>EXPORT FINISHED</t><br />
<t color='%1' size='1.2'>with %2 errors</t><br />
<br /><br /><t size='1.2' color='#AAAAAA' v-align='bottom'>%3 LOGS %3</t>
		"
	, _errorColor, _totalErrors, LOGS_ARROW];
};

{
	private _return = [
		_display,
		_parentGrp,
		_x
	] call (uiNamespace getVariable "grad_meh_fnc_loadingItem_create");

	_return params ["_item"];

	diag_log _item;

	_item ctrlSetPositionY _yPos;
	_item ctrlCommit 0;

	private _height = (ctrlPosition _item) select 3;
	_yPos = _yPos + _height;
} forEach (_display getVariable ["grad_meh_worlds", []]);

diag_log format ["y: %1", _ypos];
diag_log format ["worlds: %1", (_display getVariable ["grad_meh_worlds", []])];