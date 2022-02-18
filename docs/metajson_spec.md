# meta.json

## 1. Introduction
This specification outlines the structure of a map's `meta.json`.

# 2. JSON Schema
A JSON Schema matching this specification can be founde [here](./meta_json_schema.json).

# 3. Fields
Inside this document the term "`MAP_CONFIG`" refers to the  [CfgWorlds](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference) class of the examined map ([`configFile`](https://community.bistudio.com/wiki/configFile)` >> "CfgWorlds" >> `[`worldName`](https://community.bistudio.com/wiki/worldName)).

| Field | Type | Example | Description |
| --- | --- | --- | --- |
| `author` | `string` | `"Bohemia Interactive"` | Map author<br>(corresponds to [`MAP_CONFIG >> "author"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#author)) |
| `displayName` | `string` | `"Stratis"` | Display name<br>(corresponds to [`MAP_CONFIG >> "description"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#description)) |
| `elevationOffset` | `number` | `0.0` | Offset (in m) of DEM values from 0 ASL<br>(corresponds to [`MAP_CONFIG >> "elevationOffset"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#elevationOffset)) |
| `gridOffsetX` | `number` | `0.0` | Offset (in m) of grid origin in X direction [(1)](#1)<br>(corresponds to `MAP_CONFIG >> "Grid" >> "offsetX"`) |
| `gridOffsetY` | `number` | `8192.0` | Offset (in m) of grid origin in Y direction [(1)](#1)<br>(corresponds to `MAP_CONFIG >> "Grid" >> "offsetY"`) |
| `grids` | `array` | `[]` | Includes grids for different zoom levels;<br>(Has an item for every sub class of `MAP_CONFIG >> "Grid"`) |
| `grids[i].format` | `string` | `"XY"` | General grid format; `X` will be replaced by the formatted x-value and `Y` by the formatted y-value<br>(corresponds to `MAP_CONFIG >> "Grid" >> <ZOOM> >> "format"`) |
| `grids[i].formatX` | `string` | `"00"` | Format of x value [(2)](#2)<br>(corresponds to `MAP_CONFIG >> "Grid" >> <ZOOM> >> "formatX"`)  |
| `grids[i].formatY` | `string` | `"00"` | Format of y value [(2)](#2)<br>(corresponds to `MAP_CONFIG >> "Grid" >> <ZOOM> >> "formatY"`) |
| `grids[i].stepX` | `number` | `1000` | Spacing between grid steps (in m) on x-axis [(1)](#1)<br>(corresponds to `MAP_CONFIG >> "Grid" >> <ZOOM> >> "stepX"`) |
| `grids[i].stepY` | `number` | `-1000` | Spacing between grid steps (in m) on y-axis [(1)](#1)<br>(corresponds to `MAP_CONFIG >> "Grid" <ZOOM> >> "stepY"`) |
| `grids[i].zoomMax` | `number` | `0.949999988079071` | Max zoom at which grid is shown (same as [ctrlMapScale](https://community.bistudio.com/wiki/ctrlMapScale))<br>(corresponds to `MAP_CONFIG >> "Grid" >> <ZOOM> >> "zoomMax"`)  |
| `latitude` | `number` | `-35.09700012207031` | Latitude of map<br>(corresponds to [`MAP_CONFIG >> "latitude"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#latitude)) |
| `longitude` | `number` | `16.8200035095215` | Longitude of map<br>(corresponds to [`MAP_CONFIG >> "longitude"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#longitude)) |
| `colorOutside` | `array` | `[0.227451, 0.27451, 0.384314, 1]` | Outside color of map. _This property is optional and may be omitted, if the related config attribute does not exist._<br>(corresponds to [`MAP_CONFIG >> "OutsideTerrain" >> "colorOutside"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#class_OutsideTerrain)) |
| `version` | `number` | `0.1` | Version of `grad_meh` | |
| `worldName` | `string` | `"stratis"` | ID of map<br>(corresponds to LOWERCASE [`configName`](https://community.bistudio.com/wiki/configName) `MAP_CONFIG`) |
| `worldSize` | `number` | `8192` | Size of map in meters<br>(corresponds to  [`MAP_CONFIG >> "mapSize"`](https://community.bistudio.com/wiki/Arma_3_CfgWorlds_Config_Reference#mapSize)) |

### Footnotes

#### (1)
Contrary to most coordinate systems (including the Arma 3 position system) the Arma 3 grid system has its origin in the top left corner whilst the x coordinates go from left to right and the y coordinates go from top to bottom.  
![](./assets/grid_coord_system.svg)


#### (2)
_(Little disclaimer: We're not completely sure if the following is 100% correct. We couldn't find proper documentation for Arma 3's grid formats and extrapolated the following from just trying out a couple of formats ourselves. Hit us up, if you find any proper documentation!)_  
Looks like Arma replaces the first ten uppercase letters of the alphabet (so A-J) as well as all numbers (0-9) with coordinates.  
Similar to normal counting systems the rightmost digit will be increased 10 times before the next digit - to the left of that - will be increased.
The procedure for a single digit looks like this:   
If you have a `2`, the first grid (from the grid offset) will have a `2`. Arma will then count up to `9` and then go back to `0` and count the remaining numbers (in this case `0` and `1`). The uppercase letters behave similar. It always counts from the letter within the format string up to `J` (10th letter in the alphabet) and then start from `A` again, before moving to the next significant figure.  
For example the format `"B3"` will produce the following grid coordinates:
`B3`, `B4`, `B5`, `B6`, `B7`, `B8`, `B9`, `B0`, `B1`, `B2`, `C3`, `C4`, `C5`, ... , `J2`, `A3`, `A4`, `A5`, `A6`, `A7`, `A8`, `A9`, `A0`, `A1`, `A2` 
