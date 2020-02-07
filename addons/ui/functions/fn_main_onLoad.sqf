/*
 * Author: DerZade
 * Triggerd by the "onLoad"-EH of the main display
 * Adds all maps to display
 *
 * Arguments:
 * 0: Display <DISPLAY>
 *
 * Return Value:
 * NONE
 *
 * Example:
 * _this call grad_meh_fnc_onLoad;
 *
 * Public: No
 */
#include "../idcmacros.hpp"

params ["_display"];

private _mapList = _display displayCtrl IDC_DIALOG_CONTENT;

private _worlds = ("true" configClasses (configFile >> "CfgWorldList"));
private _selectedMaps = uiNamespace getVariable ["grad_meh_selectedMaps", []];

// calculate how many colums fit into list
private _listW = ((ctrlPosition _mapList) select 2) / GRID_W;
private _columns = floor (_listW / MAP_ITEM_W);
_columns = _columns min (count _worlds);

// calculate padding of list
private _sidePadding = (_listW - _columns * MAP_ITEM_W) / 2;

{
	private _worldName = configName _x;
	private _config = (configFile >> "CfgWorlds" >> _worldName);
	private _displayName = [_config, "description", ""] call (uiNamespace getVariable "BIS_fnc_returnConfigEntry");
	private _image = [_config, "pictureMap", ""] call (uiNamespace getVariable "BIS_fnc_returnConfigEntry");
	private _author = [_config, "author", ""] call (uiNamespace getVariable "BIS_fnc_returnConfigEntry");
	private _selected = _worldName in _selectedMaps;

	private _item = [
		_display,
		_mapList,
		_worldName,
		_image,
		_displayName,
		_author,
		_selected
	] call (uiNamespace getVariable "grad_meh_fnc_mapItem_create");

	// update position of map item
	private _row = floor (_forEachIndex / _columns);
	private _col = _forEachIndex % _columns;
	_item ctrlSetPosition [
		(_sidePadding + _col * MAP_ITEM_W) * GRID_W, 	// x
		(_row * MAP_ITEM_H + SPACING) * GRID_H			// y
	];
	_item ctrlCommit 0;
} forEach _worlds;