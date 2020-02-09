# meta.json

## 1. Introduction
This specification outlines the structure of the `meta.json` of a map.

The following table explains each field. 

// TODO: explain `MAP_CONFIG` keyword

| Field | Type | Example | Description |
| --- | --- | --- | --- |
| `displayName` | `string` | `"Stratis"` | Display name<br>(corresponds with [`MAP_CONFIG > description`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#description)) |
| `elevationOffset` | `number` | `0.0` | Offset (in m) of DEM values from 0 ASL<br>(corresponds with [`MAP_CONFIG > elevationOffset`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#elevationOffset)) |
| `gridOffsetX` | `number` | `0.0` | Offset (in m) of grid start from top left corner in X direction<br>(corresponds with `MAP_CONFIG > "Grid" > "offsetX"`) |
| `gridOffsetY` | `number` | `8192.0` | Offset (in m) of grid start from top left corner in Y direction<br>(corresponds with `MAP_CONFIG > "Grid" > "offsetY"`) |
| `grids` | `array` | `[]` | TODO |
| `grids[x].format` | `string` | `"XY"` | TODO |
| `grids[x].formatX` | `string` | `"00"` | TODO |
| `grids[x].formatY` | `string` | `"00"` | TODO |
| `grids[x].stepX` | `number` | `1000` | TODO |
| `grids[x].stepY` | `number` | `-1000` | TODO |
| `grids[x].zoomMax` | `number` | `0.949999988079071` | TODO |
| `latitude` | `number` | `-35.09700012207031` | Latitude of map<br>(corresponds with [`MAP_CONFIG > latitude`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#latitude)) |
| `longitude` | `number` | `16.8200035095215` | Longitude of map<br>(corresponds with [`MAP_CONFIG > longitude`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#longitude)) |
| `version` | `number` | `0.1` | Version of `grad_meh` | |
| `worldName` | `string` | `"Stratis"` | ID of map<br>(corresponds with [`configName`](https://community.bistudio.com/wiki/configName) `MAP_CONFIG`) |
| `worldSize` | `number` | `8192` | Size of map in meter<br>(corresponds with  [`MAP_CONFIG > mapSize`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#mapSize)) |
