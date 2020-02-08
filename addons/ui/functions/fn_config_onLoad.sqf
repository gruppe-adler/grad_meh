#include "../idcmacros.hpp"

params ["_display", "_exitCode"];

private _options = uiNamespace getVariable ["grad_meh_options",  [true, true, true, true, true]];

_options params ["_exportSat", "_exportHouses", "_exportPreviewImg", "_exportMeta", "_exportDem"];

(_display displayCtrl IDC_CONFIG_CHECK_SAT) cbSetChecked _exportSat;
(_display displayCtrl IDC_CONFIG_CHECK_HOUSES) cbSetChecked _exportHouses;
(_display displayCtrl IDC_CONFIG_CHECK_PREVIEW) cbSetChecked _exportPreviewImg;
(_display displayCtrl IDC_CONFIG_CHECK_META) cbSetChecked _exportMeta;
(_display displayCtrl IDC_CONFIG_CHECK_DEM) cbSetChecked _exportDem;
