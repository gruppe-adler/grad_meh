# Output

## 1. Introduction
This specification outlines the structure of the output directory of a single map.

## 2. Overview
```
├── dem.asc.gz
├── geojson
│   ├── bush.geojson.gz
│   ├── [...]
│   └── house.geojson.gz
├── meta.json
├── preview.png
└── sat
    ├── 0
    │   ├── [...]
    │   └── 0.png
    └── 3
        ├── [...]
        └── 0.png
```

## 3. `geojson/` directory
The `geojson/` directory holds the vector data of all objects and locations as multiple gzipped [geojson files](https://en.wikipedia.org/wiki/GeoJSON). Specifics can be found [here](./geojson_spec.md).


## 4. `meta.json`
The `meta.json` includes all important meta data of the map. The format of this file is further specified [here](./metajson_spec.md).  

## 5. `sat/` directory
The `sat/` directory includes the satellite image. Because the full image can easily get over 200MB in size (and large files are always a pain in the ass to handle) it is always split in 16 tiles.  

Each tile within the `sat/` directory has the following nomenclature `{col}/{row}.png` with `{col}` being the column number and `{row}` the row number of the tile. Row/column numbers start at 0 and the origin is in the top left corner of the sat-image. So the image `sat/0/0.png` is the most top left tile of the satellite image.   

The following image explains this better than anything written ever could:  
![](./assets/sat_tiles.svg)  
  
## 6. `dem.asc.gz`
The `dem.asc.gz` includes the [digital elevation model](https://en.wikipedia.org/wiki/Digital_elevation_model) of the map. The file is gzipped ascii file and in the [ESRI ASCII Raster Format](https://desktop.arcgis.com/de/arcmap/10.3/manage-data/raster-and-images/esri-ascii-raster-format.htm). 

## 7. `preview.png`
The `preview.png` is the maps preview image (image shown in the in-game map selection screen of the editor).
