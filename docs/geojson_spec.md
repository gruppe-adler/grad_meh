# `geojson/` directory

## 1. Introduction
This specification outlines the exact structure of the `geojson/` directory of a map.

## 2. General
The `geojson/` directory holds the vector data of all objects and locations as multiple [geojson files](https://en.wikipedia.org/wiki/GeoJSON).  
Each file is gzipped and holds an array of [geojson features](https://tools.ietf.org/html/rfc7946#section-3.2).  
Not every map has every type of object/location therefore the `geojson/` directory may include all, some or even none of the files specified below. For example Stratis doesn't include any fountains; ergo the `fountain.geojson.gz` is omitted entirely.

## 3. Generic Objects
Generic objects include all objects that are represented by an icon on the ingame map. Each object type has its own file.  
We export the following types: `bunker`, `chapel`, `church`, `cross`, `fuelstation`, `lighthouse`, `rock`, `shipwreck`, `transmitter`, `watertower`, `fortress`, `fountain`, `view-tower`, `quay`, `hospital`, `busstop`, `stack`, `ruin`, `tourism`, `powersolar`, `powerwave`, `powerwind`, `tree`, `bush`

- File: `<OBJECT_TYPE>.geojson.gz`
- Geometry-Type: [Point](https://tools.ietf.org/html/rfc7946#section-3.1.2)
- Properties: none

## 4. Houses
Houses include all objects, of which the boundaries should be drawn onto the map. 

- File: `house.geojson.gz`
- Geometry-Type: [Polygon](https://tools.ietf.org/html/rfc7946#section-3.1.6)
- Properties:
    - `color`: Array of three integers from 0 to 255 `[red, green, blue]`
    - `height`: Bounding box height in meters
    - `position`: Array of three floats `[x, y, z]`

## 5. Rocks
Rocks are [those grey areas](./assets/rocks.png) that are drawn onto the ingame map.
- File: `rocks.geojson.gz`
- Geometry-Type: [MultiPolygon](https://tools.ietf.org/html/rfc7946#section-3.1.7)
- Properties: none

## 6. Forests
Similar to [Rocks](#Rocks), Forests include the green areas that are drawn onto the ingame map.
- File: `forest.geojson.gz`
- Geometry-Type: [MultiPolygon](https://tools.ietf.org/html/rfc7946#section-3.1.7)
- Properties: none

## 7. Railways
- File: `railway.geojson.gz`
- Geometry-Type: [LineString](https://tools.ietf.org/html/rfc7946#section-3.1.4)
- Properties: none

## 8. Power Lines
- File: `powerline.geojson.gz`
- Geometry-Type: [LineString](https://tools.ietf.org/html/rfc7946#section-3.1.4)
- Properties: none

## 9. Runways
- File: `runway.geojson.gz`
- Geometry-Type: [Polygon](https://tools.ietf.org/html/rfc7946#section-3.1.6)
- Properties: none

## 10. Locations
Arma 3 suports a lot of different [location types](https://community.bistudio.com/wiki/Location#Location_Types). Each type has its own file in the `locations/` subdirectory. The filename corresponds to the lowercase location type.  
→ All locations of type `NameLocal` can be found in `locations/namelocal.geojson.gz`

- File: `locations/<LOCATON_TYPE>.geojson.gz`
- Geometry-Type: [Point](https://tools.ietf.org/html/rfc7946#section-3.1.2)
- Properties:
    - `angle`: Corresponds to value in map config
    - `name`: Corresponds to value in map config
    - `radiusA`: Corresponds to value in map config
    - `radiusB`: Corresponds to value in map config

## 11. Roads
Arma 3 splits roads into different types. These types are usually only `main_road`, `road`, `track` and `trail` but this may be subject to change with new/modded maps. Each road type has its own file in the `roads/` subdirectory. The filename corresponds to the lowercase road type.  
→ All roads of type `main_road` can be found in `roads/main_road.geojson.gz`

- File: `roads/<ROAD_TYPE>.geojson.gz`
- Geometry-Type: [LineString](https://tools.ietf.org/html/rfc7946#section-3.1.4)
- Properties:
    - `width`: Width factor of road (is extracted from `RoadsLib.cfg`)

## 12. Bridges
Bridges can be split in the same categories as [roads](#11-roads). Like [roads](#11-roads) they can be found in the `roads/` subdirectory and the filename also corresponds to the lowercase road type but with a `-bridge` suffix.  
→ All bridges of type `track` can be found in `roads/track-bridge.geojson.gz`

- File: `roads/<ROAD_TYPE>-bridge.geojson.gz`
- Geometry-Type: [Polygon](https://tools.ietf.org/html/rfc7946#section-3.1.6)
- Properties: none

## 13. Rivers
- File: `river.geojson.gz`
- Geometry-Type: [Polygon](https://tools.ietf.org/html/rfc7946#section-3.1.6)
- Properties: none

## 14. Mounts
- File: `mounts.geojson.gz`
- Geometry-Type: [Point](https://tools.ietf.org/html/rfc7946#section-3.1.2)
- Properties:
    - `elevation`: The height of the location