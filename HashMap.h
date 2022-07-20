//
// Created by yyh on 22-7-11.
//

#ifndef CENTRAL_VOTING_HASHMAP_H
#define CENTRAL_VOTING_HASHMAP_H
#include <pcl/features/normal_3d.h>
#include <pcl/features/ppf.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/registration/ppf_registration.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/visualization/pcl_visualizer.h>

#include <pcl/features/fpfh.h>
#include <pcl/features/moment_of_inertia_estimation.h>
#include <pcl/features/normal_3d.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/pcd_io.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/ia_ransac.h>
#include <pcl/visualization/cloud_viewer.h>
#include <Eigen/Core>
#include <fstream>
#include <limits>
#include <pcl/search/impl/search.hpp>
#include <utility>
#include <vector>
#include "Eigen/Core"
#include "pcl/console/print.h"
#include "pcl/point_cloud.h"
#include "pcl/point_representation.h"
#include "pcl/registration/registration.h"
#include "pcl/visualization/cloud_viewer.h"
namespace Hash{
struct HashData {
  std::pair<Eigen::Vector4f, std::pair<Eigen::Vector4f, Eigen::Vector4f>>Or;
  std::pair<Eigen::Vector4f, std::pair<Eigen::Vector4f, Eigen::Vector4f>>Ot;
};
struct HashKey {
  int k1;
  int k2;
  int k3;
  int k4;
  bool operator==(const HashKey &k) const {
    return k1 == k.k1 && k2 == k.k2 && k3 == k.k3 && k4 == k.k4;
  }
};

class HashMap {
 public:
  bool addInfo(Hash::HashKey &key, Hash::HashData &data);

  decltype(auto) find(Hash::HashKey &key);

  decltype(auto) begin();

  bool empty();

 private:


  struct hash_cal {
    size_t operator()(const HashKey &k) const {
      return std::hash<int>()(k.k1) ^ std::hash<int>()(k.k2) ^
             std::hash<int>()(k.k3) ^ std::hash<int>()(k.k4);
    }
  };
  std::unordered_multimap<HashKey, HashData, hash_cal> map;
};
typedef boost::shared_ptr<Hash::HashMap> Ptr;

}
#endif  // CENTRAL_VOTING_HASHMAP_H