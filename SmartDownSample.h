//
// Created by yyh on 22-7-12.
//

#ifndef CENTRAL_VOTING_SMARTDOWNSAMPLE_H
#define CENTRAL_VOTING_SMARTDOWNSAMPLE_H
#include "Eigen/Core"
#include "pcl/point_cloud.h"
#include "pcl/point_types.h"
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

#include <pcl/console/parse.h>  //pcl控制台解析
#include <pcl/io/pcd_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/icp.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <Eigen/Core>
#include <boost/thread/thread.hpp>
#include <iostream>
#include <thread>
#include "pcl/filters/filter.h"
#include "pcl/features/moment_of_inertia_estimation.h"
#include "pcl/impl/point_types.hpp"
#include "pcl/features/normal_3d.h"
#include "pcl/search/kdtree.h"
class SmartDownSample {
 public:
  SmartDownSample(const pcl::PointCloud<pcl::PointXYZ>::Ptr  & input_cloud,
                  const std::pair<double, double> x_range,
                  const std::pair<double, double> y_range,
                  const std::pair<double, double> z_range, const float &step,
                  const float angleThreshold, const float &distanceThreshold)
      : input_cloud(input_cloud),
        x_range(x_range),
        y_range(y_range),
        z_range(z_range),
        step(step),
        angleThreshold(angleThreshold),
        distanceThreshold(distanceThreshold){};

  pcl::PointCloud<pcl::PointNormal>::Ptr compute();

  void setRadius(float data);

  /** \brief Calculate the distance between two points
    * \param[in] points
   */
  template <class T>
  float calculateDistance(T& pointA, T&pointB);

 private:
  pcl::PointCloud<pcl::PointXYZ>::ConstPtr input_cloud;
  std::pair<double, double> x_range;
  std::pair<double, double> y_range;
  std::pair<double, double> z_range;
  float step;
  float angleThreshold, distanceThreshold;
  float normal_estimation_search_radius;
};

#endif  // CENTRAL_VOTING_SMARTDOWNSAMPLE_H