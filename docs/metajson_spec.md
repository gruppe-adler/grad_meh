# meta.json

## 1. Introduction
This specification outlines the structure of the `meta.json` of a map.

# 2. Fields
With `MAP_CONFIG` the [CfgWorlds](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference) class of the map is meant ([`configFile`](https://community.bistudio.com/wiki/configFile) >> `"CfgWorlds"` >> [`worldName`](https://community.bistudio.com/wiki/worldName)).

| Field | Type | Example | Description |
| --- | --- | --- | --- |
| `displayName` | `string` | `"Stratis"` | Display name<br>(corresponds with [`MAP_CONFIG >> "description"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#description)) |
| `elevationOffset` | `number` | `0.0` | Offset (in m) of DEM values from 0 ASL<br>(corresponds with [`MAP_CONFIG >> "elevationOffset"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#elevationOffset)) |
| `gridOffsetX` | `number` | `0.0` | Offset (in m) of grid origin in X direction<br>(corresponds with `MAP_CONFIG >> "Grid" >> "offsetX"`) [(1)](#Footnotes) |
| `gridOffsetY` | `number` | `8192.0` | Offset (in m) of grid origin in Y direction<br>(corresponds with `MAP_CONFIG >> "Grid" >> "offsetY"`) [(1)](#Footnotes) |
| `grids` | `array` | `[]` | Grids per zoom level; Has an item for every sub class of `MAP_CONFIG >> "Grid"` |
| `grids[x].format` | `string` | `"XY"` | General grid format; `X` will be replaced by the formatted x-value and `Y` by the formatted y-value |
| `grids[x].formatX` | `string` | `"00"` | Format of x value. Each character will be replaced. /* TODO */  |
| `grids[x].formatY` | `string` | `"00"` | Format of y value. For further info see `grids[x].formatX`. |
| `grids[x].stepX` | `number` | `1000` | Spacing between grid steps (in m) on x-axis [(1)](#Footnotes) |
| `grids[x].stepY` | `number` | `-1000` | Spacing between grid steps (in m) on y-axis. [(1)](#Footnotes) |
| `grids[x].zoomMax` | `number` | `0.949999988079071` | Max zoom at which grid is shown (same as [ctrlMapScale](https://community.bistudio.com/wiki/ctrlMapScale))  |
| `latitude` | `number` | `-35.09700012207031` | Latitude of map<br>(corresponds with [`MAP_CONFIG >> "latitude"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#latitude)) |
| `longitude` | `number` | `16.8200035095215` | Longitude of map<br>(corresponds with [`MAP_CONFIG >> "longitude"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#longitude)) |
| `version` | `number` | `0.1` | Version of `grad_meh` | |
| `worldName` | `string` | `"Stratis"` | ID of map<br>(corresponds with [`configName`](https://community.bistudio.com/wiki/configName) `MAP_CONFIG`) |
| `worldSize` | `number` | `8192` | Size of map in meters<br>(corresponds with  [`MAP_CONFIG >> "mapSize"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#mapSize)) |

### Footnotes
| ID | Description |
| --- | --- |
| (1) | Contrary to most coordinate systems (including the Arma 3 position coordinate system) the Arma 3 grid coordinate system has its origin in the top left corner whilst the x coordinates go from left to right and the y coordinates go from top to bottom.<br>![](./assets/grid_coord_system.svg) |