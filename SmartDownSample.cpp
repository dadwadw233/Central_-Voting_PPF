//
// Created by yyh on 22-7-12.
//

#include "SmartDownSample.h"
#include <omp.h>
pcl::PointCloud<pcl::PointNormal>::Ptr SmartDownSample::compute() {
  std::cout << "before down sample" << this->input_cloud->points.size();
  PCL_INFO("\nstart to compute\n");
  PCL_INFO("\nstart to calculate normal\n");
  pcl::PointCloud<pcl::Normal>::Ptr normal(new pcl::PointCloud<pcl::Normal>());
//计算所有点的表面法线
  pcl::NormalEstimationOMP<pcl::PointXYZ, pcl::Normal> normal_estimation_filter;
  normal_estimation_filter.setInputCloud(this->input_cloud);
  pcl::search::KdTree<pcl::PointXYZ>::Ptr search_tree(
      new pcl::search::KdTree<pcl::PointXYZ>);  ////建立kdtree来进行近邻点集搜索
  normal_estimation_filter.setSearchMethod(search_tree);
  if(isSetViewPoint){//自定义视点
    normal_estimation_filter.setViewPoint(view_point[0], view_point[1], view_point[2]);
  }

  if (isSetRadius) {//两种不同的估计方法
    normal_estimation_filter.setRadiusSearch(normal_estimation_search_radius);
  } else {
    normal_estimation_filter.setKSearch(normal_estimation_search_k_points);
  }
  normal_estimation_filter.compute(*normal);
  if (reverse) {
    for (auto &i : *normal) {
      i.normal_x = -i.normal_x;
      i.normal_y = -i.normal_y;
      i.normal_z = -i.normal_z;
      i.normal[0] = i.normal_x;
      i.normal[1] = i.normal_y;
      i.normal[2] = i.normal_z;
      i.curvature = -i.curvature;
    }
  }
  pcl::PointCloud<pcl::PointNormal>::Ptr cloud_with_normals(
      new pcl::PointCloud<pcl::PointNormal>());
  concatenateFields(*this->input_cloud, *normal, *cloud_with_normals);
#pragma omp barrier
  PCL_INFO("\nfinish normal calculation\n");
  pcl::PointCloud<pcl::PointNormal>::Ptr output_cloud(
      new pcl::PointCloud<pcl::PointNormal>());

  std::vector<pcl::PointCloud<pcl::PointNormal>::Ptr>
      map;  // define the voxel grid , store the index of the point in cloud
  PCL_INFO("\nstart map init\n");

  auto xr =
      std::abs(static_cast<float>(this->x_range.second - this->x_range.first));
  auto yr =
      std::abs(static_cast<float>(this->y_range.second - this->y_range.first));
  auto zr =
      std::abs(static_cast<float>(this->z_range.second - this->z_range.first));
  //确定三个方向上的栅格数量
  auto x_num = static_cast<long long int>(std::ceil(xr / this->step));
  auto y_num = static_cast<long long int>(std::ceil(yr / this->step));
  auto z_num = static_cast<long long int>(std::ceil(zr / this->step));

  map.resize(x_num * y_num * z_num);

#pragma omp parallel shared(map) default(none)
  {
#pragma omp for
    for (int i = 0; i < map.size(); i++) {
#pragma omp critical
      map[i].reset(new pcl::PointCloud<pcl::PointNormal>());
    }
  }

#pragma omp barrier
  PCL_INFO("\nfinish voxel init\n");

#pragma omp parallel for shared(map, x_num, y_num, cloud_with_normals, \
                                cout) default(none) num_threads(15)
  for (int i = 0; i < this->input_cloud->points.size(); i++) {//遍历所有点，分配至对应cell
    const int xCell =
        static_cast<int>(std::ceil(
            (input_cloud->points[i].x - this->x_range.first) / step)) == 0
            ? 1
            : static_cast<int>(std::ceil(
                  (input_cloud->points[i].x - this->x_range.first) / step));
    const int yCell =
        static_cast<int>(std::ceil(
            (input_cloud->points[i].y - this->y_range.first) / step)) == 0
            ? 1
            : static_cast<int>(std::ceil(
                  (input_cloud->points[i].y - this->y_range.first) / step));
    const int zCell =
        static_cast<int>(std::ceil(
            (input_cloud->points[i].z - this->z_range.first) / step)) == 0
            ? 1
            : static_cast<int>(std::ceil(
                  (input_cloud->points[i].z - this->z_range.first) / step));

    const int index =
        (xCell - 1) + (yCell - 1) * x_num + (zCell - 1) * x_num * y_num;//确定点所在的cell的index

#pragma omp critical
    map[index]->points.push_back(cloud_with_normals->points[i]);

  }

#pragma omp barrier

  PCL_INFO("\nfinish store point\n");

#pragma omp parallel for shared(map, output_cloud, cout) default(none) \
    num_threads(15)
  for (int i = 0; i < map.size(); i++) {//遍历所有cell
    if (map[i]->points.empty()) {//cell为空
      continue;
    } else if (map[i]->points.size() == 1 && isdense) {//cell中只有一个点
#pragma omp critical
      output_cloud->points.push_back(map[i]->points[0]);
      continue;
    } else {//cell中有多个点
      std::vector<std::vector<pcl::PointNormal>>cluster;
      for (int j = 0; j < map[i]->points.size(); j++) {//遍历cell内的所有点
        bool inside = false;
        for(auto cluster_index = 0;cluster_index<cluster.size();cluster_index++){
          bool flag = true;
          for(auto point_index = 0;point_index<cluster[cluster_index].size();point_index++){
            if(pcl::getAngle3D(static_cast<Eigen::Vector3f>(cluster[cluster_index][point_index].normal), static_cast<Eigen::Vector3f>(map[i]->points[j].normal),true)<this->angleThreshold){
              continue;
            }else{
              flag = false;
              break;//只要有一组之间的角度差大于阈值，直接跳出
            }
          }
          if(flag){//满足并入当前group的要求
            cluster[cluster_index].push_back(map[i]->points[j]);
            inside = true;
            break;//当前点已经找到对应group，直接退出
          }else{
            continue;//在其他group中继续搜索
          }
        }
        if(inside){
          continue;
        }else{
          std::vector<pcl::PointNormal>new_cluster;
          new_cluster.push_back(map[i]->points[j]);
          cluster.push_back(new_cluster);
        }
      }
      //每一个cell中的点遍历结束之后对聚类求均值
      for(auto cluster_index = 0;cluster_index<cluster.size();cluster_index++){
        if(cluster[cluster_index].size() == 0){
#pragma omp critical
          output_cloud->points.push_back(cluster[cluster_index][0]);
        }else{
          auto Mean = getMeanPointNormal(cluster[cluster_index]);
#pragma omp critical
          output_cloud->points.push_back(Mean);
        }
      }
    }
  }
  PCL_INFO("\ndown sample finish\n");
  std::cout << "after down sample" << output_cloud->points.size() << std::endl;
  return output_cloud;
}
void SmartDownSample::setIsdense(const bool &data) { this->isdense = data; }
void SmartDownSample::setRadius(float data) {
  this->normal_estimation_search_radius = data;
  this->isSetPoints = false;
  this->isSetRadius = true;
}
template <class T>
float SmartDownSample::calculateDistance(T &pointA, T &pointB) {
  float distance = std::pow((pointA.x - pointB.x), 2) +
                   std::pow((pointA.y - pointB.y), 2) +
                   std::pow((pointA.z - pointB.z), 2);
  return distance;
}
template <class T>
float SmartDownSample::calculateDistance(T &pointA, pcl::PointNormal &pointB) {
  float distance = std::pow((pointA.x - pointB.x), 2) +
                   std::pow((pointA.y - pointB.y), 2) +
                   std::pow((pointA.z - pointB.z), 2);
  return distance;
}

void SmartDownSample::setKSearch(const int data) {
  this->normal_estimation_search_k_points = data;
  this->isSetPoints = true;
  this->isSetRadius = false;
}

