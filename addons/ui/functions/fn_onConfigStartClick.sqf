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

private _exportSat = cbChecked (_display displayCtrl IDC_CONFIG_CHECK_SAT);
private _exportHouses = cbChecked (_display displayCtrl IDC_CONFIG_CHECK_HOUSES);
private _exportPreviewImg = cbChecked (_display displayCtrl IDC_CONFIG_CHECK_PREVIEW);
private _exportMeta = cbChecked (_display displayCtrl IDC_CONFIG_CHECK_META);
private _exportDem = cbChecked (_display displayCtrl IDC_CONFIG_CHECK_DEM);

[
	_maps,
	_exportSat,
	_exportHouses,
	_exportPreviewImg,
	_exportMeta,
	_exportDem
] call (uiNamespace getVariable "grad_meh_fnc_export");
