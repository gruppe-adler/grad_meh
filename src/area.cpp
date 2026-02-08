#include "area.h"

#undef max
#undef min
// PCL
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/features/normal_3d.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>

#include <pcl/ModelCoefficients.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/features/normal_3d.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/segmentation/impl/extract_clusters.hpp>
#include <pcl/search/organized.h>
#include <pcl/search/impl/search.hpp>
#include <pcl/search/impl/organized.hpp>

#include <clipper2/clipper.h>

#include <ogr_geometry.h>

#include <nlohmann/json.hpp>

#include <cmath>

namespace nl = nlohmann;

void writeArea(arma_file_formats::cxx::OprwCxx& wrp, fs::path& basePathGeojson, const std::vector<std::pair<arma_file_formats::cxx::ObjectCxx, arma_file_formats::cxx::LodCxx&>>& objectPairs,
    uint32_t epsilon, uint32_t minClusterSize, uint32_t buffer, uint32_t simplify, const std::string& name) {

    size_t nPoints = 0;
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloudPtr(new pcl::PointCloud<pcl::PointXYZ>());
    for (auto& objectPair : objectPairs) {
        pcl::PointXYZ point;
        point.x = objectPair.first.transform_matrx._3.x;
        point.y = objectPair.first.transform_matrx._3.z;
        point.z = 0;

        if (/*point.x < 0 || */std::isnan(point.x) || std::isinf(point.x) ||
            /*point.y < 0 || */std::isnan(point.y) || std::isinf(point.y)) {
            PLOG_WARNING << fmt::format("[writeArea {}] Skipping point X: {} Y: {}", name, point.x, point.y);
            continue;
        }

        cloudPtr->push_back(point);
        nPoints++;
    }

    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    tree->setInputCloud(cloudPtr);

    std::vector<pcl::PointIndices> cluster_indices;
    pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
    ec.setClusterTolerance(epsilon); // in m
    ec.setMinClusterSize(minClusterSize);
    ec.setSearchMethod(tree);
    ec.setInputCloud(cloudPtr);
    ec.extract(cluster_indices);

    pcl::PCDWriter writer;
    Clipper2Lib::Paths64 paths;
    paths.reserve(nPoints);

    for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin(); it != cluster_indices.end(); ++it)
    {
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloudCluster(new pcl::PointCloud<pcl::PointXYZ>);
        for (std::vector<int>::const_iterator pit = it->indices.begin(); pit != it->indices.end(); ++pit)
            cloudCluster->points.push_back(cloudPtr->points[*pit]);
        cloudCluster->width = cloudCluster->points.size();
        cloudCluster->height = 1;
        cloudCluster->is_dense = true;

        for (auto& point : *cloudCluster) {
            Clipper2Lib::Path64 polygon;
            polygon.push_back(Clipper2Lib::Point64(point.x - buffer, point.y));
            polygon.push_back(Clipper2Lib::Point64(point.x, point.y - buffer));
            polygon.push_back(Clipper2Lib::Point64(point.x + buffer, point.y));
            polygon.push_back(Clipper2Lib::Point64(point.x, point.y + buffer));
            paths.push_back(polygon);
        }
    }

    //Clipper2Lib::Paths64 solution;
    //Clipper2Lib::Clip c;
    //c.AddPaths(paths, Clipper2Lib::ptSubject, true);
    //c.Execute(Clipper2Lib::ctUnion, solution, Clipper2Lib::pftNonZero);
    Clipper2Lib::Paths64 solution = Clipper2Lib::Union(paths, Clipper2Lib::FillRule::NonZero);

    std::vector<OGRLinearRing*> rings;
    rings.reserve(solution.size());

    for (auto& path : solution) {
        auto ring = new OGRLinearRing();
        for (auto& point : path) {
            ring->addPoint(point.x, point.y);
        }
        ring->closeRings();
        rings.push_back(ring);
    }

    std::vector<OGRGeometry*> polygons;
    polygons.reserve(rings.size());
    for (auto& ring : rings) {
        OGRPolygon poly;
        poly.addRing(ring);
        polygons.push_back(poly.Simplify(3));
        OGRGeometryFactory::destroyGeometry(ring);
    }

    OGRMultiPolygon multiPolygon;
    for (auto& polygon : polygons) {
        multiPolygon.addGeometryDirectly(polygon);
    }

    OGRGeometry* multiPolygonPtr = nullptr;
    for (int i = simplify; i > 0; i--) {
        auto simplifyPtr = multiPolygon.Simplify(simplify);
        if (simplifyPtr != nullptr) {
            multiPolygonPtr = simplifyPtr;
            break;
        }
    }

    if (multiPolygonPtr == nullptr) {
        return;
    }

    auto unionPtr = multiPolygonPtr->UnionCascaded();
    if (unionPtr != nullptr) {
        multiPolygonPtr = unionPtr;
    }

    nl::json area;

    nl::json forest;
    forest["type"] = "Feature";

    auto ogrJson = multiPolygonPtr->exportToJson();
    forest["geometry"] = nl::json::parse(ogrJson);
    CPLFree(ogrJson);
    OGRGeometryFactory::destroyGeometry(multiPolygonPtr);

    forest["properties"] = nl::json::object();
    area.push_back(forest);

    writeGZJson(name + ".geojson.gz", basePathGeojson, area);
}
