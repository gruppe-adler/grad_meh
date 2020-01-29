# Output

## Introduction
This specification outlines the structure of the output directory of a single map.

## Overview
![](./assets/output_dir_structure.svg)

## `geojson/houses.geojson.gz`
// TODO

## `meta.json`
The `meta.json` includes all important meta data of the map. The format of this file is further specified [here](./metajson_spec.md).  

## `sat/`
The `sat/` directory includes the satellite image. Because the full image can easily get over 100MB in size it is split (always) in 16 tiles.  
  
Each tile within the `sat/` directory has the following nomenclature `{col}/{row}.png` with `{col}` being the column number and `{row}` the row number of the tile. Row/column numbers start at 0 and the origin is in the top left corner of the sat-image. So the image `sat/0/0.png` is the most top left tile of the satellite image.   
The following image explains this better than anything written ever could:
![](./assets/sat_tiles.svg)
  
## `dem.asc.gz`
The `dem.asc.gz` includes the [Digital Elevation Model](https://en.wikipedia.org/wiki/Digital_elevation_model) of the map. The file is gzipped and in the [ESRI ASCII Raster Format](https://desktop.arcgis.com/de/arcmap/10.3/manage-data/raster-and-images/esri-ascii-raster-format.htm). 

## `preview.png`
The `preview.png` is the maps preview image (image shown in the in-game map selection screen of the editor).
