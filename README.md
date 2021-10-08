# Gruppe Adler Map Exporter

Gruppe Adler Map Exporter (grad_meh) is an Arma 3 modification built on [intercept](https://github.com/intercept/intercept), which allows exporting Arma 3 maps.  
If you wonder what the `h` stands for you should check out [this pull request](https://github.com/gruppe-adler/grad_meh/pull/1).   

The aim of this project is not to generate finished map tiles for a [WMTS](https://en.wikipedia.org/wiki/Web_Map_Tile_Service), but to export all data that might be needed to generate such tiles or basically anything else. So the goal is to export everything interesting about an Arma 3 map. What you do with that data is up to you. The use cases are only limited by your imagination (ﾉ◕ヮ◕)ﾉ*:・ﾟ✧  
  
The [output section](#Output) outlines what exactly is exported.  
  
Check out our [FAQ](#FAQ) at the bottom of this README if you ask yourself "How?" or "Why?".

## Installation
Download the latest version of the mod from our [releases page](https://github.com/gruppe-adler/grad_meh/releases). The only dependency, **which has to be loaded as well** is [intercept](https://steamcommunity.com/sharedfiles/filedetails/?id=1645973522). 

## Usage
Although we would say that using `grad_meh` is intuitive and mostly self-explanatory, we made <a href="https://youtu.be/VdIXrm_eUMc" target="_blank">this short video</a> on how to use it.  
**HINT**: If you have any problems with the export not starting, try disabling BattlEye.

## Limitations
- To reduce complexity we only allow one export process at a time.
- To reduce complexity it is not possible to stop a running export reliably, except by killing the game.
- MEH cannot export maps included in `.ebo` files, because those are encrypted and therefore we cannot get to the map files. 

## Output
We export the following from Arma 3 maps:
- Elevation model
- Preview picture
- Satellite image
- Vector data for all map objects / locations
- Any interesting metadata like display name, longitude, latitude, etc.

All exported files can be found in the `grad_meh` subdirectory of your Arma 3 installation directory. Each map has its own subdirectory, which structure is further explained [here](./docs/output_spec.md).

## Scripting API
Gruppe Adler Map Exporter adds a script command, which is used by the UI. If you want to, you could leverage that scripting command for your own purposes.

### `gradMehExportMap`
|**Syntax**| |  
|---|---|  
|Description| Start export of given map. |
|||
|Syntax| **gradMehExportMap** [mapId, sat, geojson, previewImg, meta, dem]
|||
|Parameters|[mapId, sat, geojson, previewImg, meta, dem]: [Array](https://community.bistudio.com/wiki/Array)|
||mapId: [String](https://community.bistudio.com/wiki/String) - CfgWorlds class name of map|
||sat: [Boolean](https://community.bistudio.com/wiki/Boolean) - Export sat images|
||geojson: [Boolean](https://community.bistudio.com/wiki/Boolean) - Export GeoJSONs|
||previewImg: [Boolean](https://community.bistudio.com/wiki/Boolean) - Export preview image|
||meta: [Boolean](https://community.bistudio.com/wiki/Boolean) - Export meta.json|
||dem: [Boolean](https://community.bistudio.com/wiki/Boolean) - Export Digital Elevation Model|
|||
|Return Value| [Number](https://community.bistudio.com/wiki/Number) - Status code<br>`0` - Export process started nominally<br>`1` - Error: Invalid arguments given<br>`2` - Error: Another export is currently running<br>`3` - Error: Map wasn't found in config file<br>`4` - Error: Map invalid because `worldSize` field in config is missing<br>`5` - Error: PBO of map's WRP not found (Most likely because it is an EBO)<br>`6` - Error: PBO map still populating (MEH has to do this once upon game start, before it can export anything)
|||
|Examples|`gradMehExportMap ["Stratis", true, true, true, true, true];`|  

## FAQ

### How long does an export take?
How long an export takes varies a lot depending on your machine and factors like map size, map object count and which data you want to export.  
But for reference: It took us less than thirty seconds to export everything for Stratis on a normal gaming system (i7-6700K@4.00Ghz, NVIDIA GeForce GTX 1060 6GB, 16 GB RAM).

### Is there some exported data already available for download?
Yes, check out [this repository](https://github.com/gruppe-adler/meh-data). (We would appreciate if you contribute exported maps to this repository)

### Can I just tab out while the export process is running?
Yes you can tab out while the export is running, but we recommend starting with the [`-noPause` Arma 3 startup parameter](https://community.bistudio.com/wiki/Arma_3_Startup_Parameters#Developer_Options). Otherwise, MEH will not initiate the export of pending maps.  
Oh... and yeah and your CPU utilization will probably peak around 100% while MEH is running so have fun being productive.

### How does grad_meh work?
grad_meh utilizes [our Arma file formats C++ library](https://github.com/gruppe-adler/grad_aff) to parse all map relevant files. So most of the magic is outsourced.  
