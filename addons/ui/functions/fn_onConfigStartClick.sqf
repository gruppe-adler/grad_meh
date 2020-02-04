/*
 * Author: DerZade
 * Triggerd by the "onMouseButtonClick"-EH of the start button of the config display
 *
 * Arguments:
 * n/a
 *
 * Return Value:
 * NONE
 *
 * Example:
 * _this call grad_meh_fnc_onConfigStartClick;
 *
 * Public: No
 */
#include "../idcmacros.hpp"

params ["_control", "_button", "_xPos", "_yPos", "_shift", "_ctrl", "_alt"];

private _display = ctrlParent _control;

private _maps = _display getVariable ["grad_meh_selectedMaps", []];

private _exportSat = ctrlChecked ((_display displayCtrl IDC_CONFIG_SAT) controlsGroupCtrl IDC_CONFIG_CHECKBOX);
private _exportHouses = ctrlChecked ((_display displayCtrl IDC_CONFIG_HOUSES) controlsGroupCtrl IDC_CONFIG_CHECKBOX);
private _exportPreviewImg = ctrlChecked ((_display displayCtrl IDC_CONFIG_PREVIEW) controlsGroupCtrl IDC_CONFIG_CHECKBOX);
private _exportMeta = ctrlChecked ((_display displayCtrl IDC_CONFIG_META) controlsGroupCtrl IDC_CONFIG_CHECKBOX);
private _exportDem = ctrlChecked ((_display displayCtrl IDC_CONFIG_DEM) controlsGroupCtrl IDC_CONFIG_CHECKBOX);

// exit if no step is seleced
if (!_exportSat && !_exportHouses && !_exportPreviewImg && !_exportMeta && !_exportDem) exitWith {};

[
	_maps,
	_exportSat,
	_exportHouses,
	_exportPreviewImg,
	_exportMeta,
	_exportDem
] call (uiNamespace getVariable "grad_meh_fnc_export");
