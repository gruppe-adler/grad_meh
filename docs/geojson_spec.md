# `geojson/` directory

## 1. Introduction
This specification outlines the exact structure of the `geojson/` directory of a map.

## 2. General
The `geojson/` directory holds the vector data of all objects and locations as multiple [geojson files](https://en.wikipedia.org/wiki/GeoJSON).  
Each file is gzipped and holds an array of [geojson features](https://tools.ietf.org/html/rfc7946#section-3.2).

## 3. Categories
All geojson files can be split in four categories:
- Generic Objects
- Special Objects
- Locations
- Roads

## 3.1 Generic Objects
Possible Types: `bunker`, `chapel`, `church`, `cross`, `fuelstation`, `lighthouse`, `rock`, `shipwreck`, `transmitter`, `tree`, `rock`, `watertower`, `fortress`, `fountain`, `view-tower`, `quay`, `hospital`, `busstop`, `stack`,`ruin`, `tourism`, `powersolar`, `powerwave`, `powerwind`, `tree`, `bush`, `rock`

- File: `<OBJECT_TYPE>.geojson.gz`
- Geomety-Type: [Point](https://tools.ietf.org/html/rfc7946#section-3.1.2)
- Properties: none

## 3.2 Special Objects
### 3.2.1 Houses
Houses are objects, which boundaries should be drawn onto the map.  
- File: `house.geojson.gz`
- Geomety-Type: [Polygon](https://tools.ietf.org/html/rfc7946#section-3.1.6)
- Properties:
    - `color`: Array of four integers from 0 to 255 `[red, green, blue, alpha]`

### 3.2.2 Rocks
Rocks are the grey areas, which are drawn on the map.
- File: `rocks.geojson.gz`
- Geomety-Type: [MultiPolygon](https://tools.ietf.org/html/rfc7946#section-3.1.7)
- Properties: none

### 3.2.3 Forests
Forests are the green areas, which are drawn on the map.
- File: `forest.geojson.gz`
- Geomety-Type: [MultiPolygon](https://tools.ietf.org/html/rfc7946#section-3.1.7)
- Properties: none

### 3.2.4 Railways
- File: `railway.geojson.gz`
- Geomety-Type: [LineString](https://tools.ietf.org/html/rfc7946#section-3.1.4)
- Properties: none

### 3.2.5 Power Lines
- File: `powerline.geojson.gz`
- Geomety-Type: [LineString](https://tools.ietf.org/html/rfc7946#section-3.1.4)
- Properties: none

### 3.2.6 Runways
- File: `runway.geojson.gz`
- Geomety-Type: [Polygon](https://tools.ietf.org/html/rfc7946#section-3.1.6)
- Properties: none

## 3.3 Locations
Arma 3 suports a lot of different [location types](https://community.bistudio.com/wiki/Location#Location_Types). Each of these types has its own file in the `locations/` subdirectory. The filename corresponds to the location type as all lowercase.  
→ All locations of type `NameLocal` can be found in `locations/namelocal.geojson.gz`

- File: `locations/<LOCATON_TYPE>.geojson.gz`
- Geometry-Type: [Point](https://tools.ietf.org/html/rfc7946#section-3.1.2)
- Properties:
    - `angle`: Corresponds to value in map config
    - `name`: Corresponds to value in map config
    - `radiusA`: Corresponds to value in map config
    - `radiusB`: Corresponds to value in map config

## 3.4 Roads
Roads come in different types. These types are usually only `main_road`, `road`, `track` and `trail` but this may be subject to change with new/modded maps. Each road type has its own file in the `roads/` subdirectory. The filename corresponds to the road type as all lowercase.  
→ All locations of type `main_road` can be found in `roads/main_road.geojson.gz`

- File: `roads/<ROAD_TYPE>.geojson.gz`
- Geometry-Type: [Polygon](https://tools.ietf.org/html/rfc7946#section-3.1.6)
- Properties: none

<!--
`bunker.geojson.gz`
`chapel.geojson.gz`
`church.geojson.gz`
`cross.geojson.gz`
`fuelstation.geojson.gz`
`lighthouse.geojson.gz`
`rock.geojson.gz`
`shipwreck.geojson.gz`
`transmitter.geojson.gz`
`tree.geojson.gz`
`rock.geojson.gz`
`watertower.geojson.gz`
`fortress.geojson.gz`
`fountain.geojson.gz`
`view-tower.geojson.gz`
`quay.geojson.gz`
`hospital.geojson.gz`
`busstop.geojson.gz`
`stack.geojson.gz`
`ruin.geojson.gz`
`tourism.geojson.gz`
`powersolar.geojson.gz`
`powerwave.geojson.gz`
`powerwind.geojson.gz`
`tree.geojson.gz`
`bush.geojson.gz`
`rock.geojson.gz`

`forest.geojson.gz`
`rocks.geojson.gz`

`railway.geojson.gz`

`runway.geojson.gz`

`powerline.geojson.gz`

`house.geojson.gz`

// LOCATIONS

// ROADS

-->