//
// Created by yyh on 22-7-26.
//

#include "PPFRegistration.h"

PPFRegistration::PPFRegistration() {
  model_cloud_with_normal.reset(new pcl::PointCloud<pcl::PointNormal>());
  scene_cloud_with_normal.reset(new pcl::PointCloud<pcl::PointNormal>());
  searchMap.reset(new Hash::HashMap());
}
void PPFRegistration::setSceneReferencePointSamplingRate(
    const float &scene_reference_point_sampling_rate) {
  this->scene_reference_point_sampling_rate =
      scene_reference_point_sampling_rate;
}

void PPFRegistration::setPositionClusteringThreshold(
    const float &clustering_position_diff_threshold) {
  this->clustering_position_diff_threshold = clustering_position_diff_threshold;
}

void PPFRegistration::setRotationClusteringThreshold(
    const float &clustering_rotation_diff_threshold) {
  this->clustering_rotation_diff_threshold = clustering_rotation_diff_threshold;
}
void PPFRegistration::setInputTarget(
    const pcl::PointCloud<pcl::PointNormal>::Ptr &cloud) {
  this->scene_cloud_with_normal = cloud;
}
void PPFRegistration::setInputSource(
    const pcl::PointCloud<pcl::PointNormal>::Ptr &cloud) {
  this->model_cloud_with_normal = cloud;
}
void PPFRegistration::setSearchMap(const Hash::Ptr &searchMap) {
  this->searchMap = searchMap;
}
void PPFRegistration::setDiscretizationSteps(
    const float &angle_discretization_step,
    const float &distance_discretization_step) {
  this->angle_discretization_step = angle_discretization_step;
  this->distance_discretization_step = distance_discretization_step;
}

Eigen::Affine3f PPFRegistration::getFinalTransformation() {
  return this->finalTransformation;
}
template <class T>
typename pcl::PointCloud<T>::Ptr PPFRegistration::aligen(
    const typename pcl::PointCloud<T>::Ptr &input) {
  typename pcl::PointCloud<T>::Ptr output =
      boost::make_shared<pcl::PointCloud<T>>();
  pcl::transformPointCloud(*input, *output, finalTransformation);
  return output;
}
void PPFRegistration::setModelTripleSet(
    const std::vector<pcl::PointXYZ> &triple_set) {
  for (size_t i = 0; i < 3; ++i) {
    this->triple_set.push_back(triple_set[i]);
  }
}
void PPFRegistration::setDobj(const float &data) { this->d_obj = data; }
void PPFRegistration::vote(const int &key, const Eigen::Affine3f &T) {
  if (map.find(key) != map.end()) {
    (map.find(key)->second).value += 1;
  } else {
    struct data d(T, 1);
    map.emplace(key, d);
  }
}

void PPFRegistration::establishVoxelGrid() {
  pcl::PointNormal max_point, min_point;
  pcl::MomentOfInertiaEstimation<pcl::PointNormal> feature_extractor;
  feature_extractor.setInputCloud(this->scene_cloud_with_normal);
  feature_extractor.compute();
  feature_extractor.getAABB(min_point, max_point);
  this->x_range = std::make_pair(min_point.x, max_point.x);
  this->y_range = std::make_pair(min_point.y, max_point.y);
  this->z_range = std::make_pair(min_point.z, max_point.z);
}

void PPFRegistration::compute() {
  // pcl::PointCloud<pcl::PointXYZ>::Ptr triple_scene =
  // boost::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  establishVoxelGrid();
  auto xr =
      std::abs(static_cast<float>(this->x_range.second - this->x_range.first));
  auto yr =
      std::abs(static_cast<float>(this->y_range.second - this->y_range.first));
  auto zr =
      std::abs(static_cast<float>(this->z_range.second - this->z_range.first));

  auto x_num = static_cast<long long int>(
      std::ceil(xr / this->clustering_position_diff_threshold));
  auto y_num = static_cast<long long int>(
      std::ceil(yr / this->clustering_position_diff_threshold));
  auto z_num = static_cast<long long int>(
      std::ceil(zr / this->clustering_position_diff_threshold));
  pcl::PPFSignature feature{};
  std::pair<Hash::HashKey, Hash::HashData> data{};
  Eigen::Vector4f p1{};
  Eigen::Vector4f p2{};
  Eigen::Vector4f n1{};
  Eigen::Vector4f n2{};
  Eigen::Vector4f delta{};
  auto tp1 = boost::chrono::steady_clock::now();
  pcl::PointCloud<pcl::PointXYZ>::Ptr triple_scene(
      new pcl::PointCloud<pcl::PointXYZ>());
  for (auto i = 0; i < scene_cloud_with_normal->points.size(); ++i) {
    for (auto j = 0; j < scene_cloud_with_normal->points.size() *
                             (this->scene_reference_point_sampling_rate / 100);
         ++j) {
      if (i == j) {
        continue;
      } else {
        p1 << scene_cloud_with_normal->points[i].x,
            scene_cloud_with_normal->points[i].y,
            scene_cloud_with_normal->points[i].z, 0.0f;
        p2 << scene_cloud_with_normal->points[j].x,
            scene_cloud_with_normal->points[j].y,
            scene_cloud_with_normal->points[j].z, 0.0f;
        n1 << scene_cloud_with_normal->points[i].normal_x,
            scene_cloud_with_normal->points[i].normal_y,
            scene_cloud_with_normal->points[i].normal_z, 0.0f;
        n2 << scene_cloud_with_normal->points[j].normal_x,
            scene_cloud_with_normal->points[j].normal_y,
            scene_cloud_with_normal->points[j].normal_z, 0.0f;

        delta = p2 - p1;
        float f4 = delta.norm();
        if (f4 > d_obj) {
          continue;
        }
        // normalize
        delta /= f4;

        float f1 = n1[0] * delta[0] + n1[1] * delta[1] + n1[2] * delta[2];

        float f2 = n1[0] * delta[0] + n2[1] * delta[1] + n2[2] * delta[2];

        float f3 = n1[0] * n2[0] + n1[1] * n2[1] + n1[2] * n2[2];

        /*float f1 = n1.x() * delta.x() + n1.y()  * delta.y() + n2.z()  *
        delta.z();

        float f2 = n1.x() * delta.x() + n2.y()  * delta.y() + n2.z()  *
        delta.z();

        float f3 = n1.x() * n2.x() + n1.y()  * n2.y()  + n1.z()  * n2.z() ;
         */
        feature.f1 = f1;
        feature.f2 = f2;
        feature.f3 = f3;
        feature.f4 = f4;
        feature.alpha_m = 0.0f;
        data.second.Or = (std::make_pair(
            n1.cross3(delta), std::make_pair(n1.cross3(n1.cross3(delta)), n1)));
        data.second.Ot = (std::make_pair(
            n2.cross3(delta), std::make_pair(n2.cross3(n2.cross3(delta)), n2)));

        data.first.k1 =
            static_cast<int>(std::floor(f1 / angle_discretization_step));
        data.first.k2 =
            static_cast<int>(std::floor(f2 / angle_discretization_step));
        data.first.k3 =
            static_cast<int>(std::floor(f3 / angle_discretization_step));
        data.first.k4 =
            static_cast<int>(std::floor(f4 / distance_discretization_step));

        data.second.r = scene_cloud_with_normal->points[i];
        data.second.t = scene_cloud_with_normal->points[j];
        if (searchMap->find(data.first)) {
          auto model_lrf = this->searchMap->getData(data.first);
          Eigen::Matrix3f model_lrf_Or;
          Eigen::Matrix3f model_lrf_Ot;
          Eigen::Matrix3f scene_lrf_Or;
          Eigen::Matrix3f scene_lrf_Ot;

          model_lrf_Or << model_lrf.Or.first[0], model_lrf.Or.second.first[0],
              model_lrf.Or.second.second[0], model_lrf.Or.first[1],
              model_lrf.Or.second.first[1], model_lrf.Or.second.second[1],
              model_lrf.Or.first[2], model_lrf.Or.second.first[2],
              model_lrf.Or.second.second[2];
          model_lrf_Ot << model_lrf.Ot.first[0], model_lrf.Ot.second.first[0],
              model_lrf.Ot.second.second[0], model_lrf.Ot.first[1],
              model_lrf.Ot.second.first[1], model_lrf.Ot.second.second[1],
              model_lrf.Ot.first[2], model_lrf.Ot.second.first[2],
              model_lrf.Ot.second.second[2];
          scene_lrf_Or << data.second.Or.first[0],
              data.second.Or.second.first[0], data.second.Or.second.second[0],
              data.second.Or.first[1], data.second.Or.second.first[1],
              data.second.Or.second.second[1], data.second.Or.first[2],
              data.second.Or.second.first[2], data.second.Or.second.second[2];
          scene_lrf_Ot << data.second.Ot.first[0],
              data.second.Ot.second.first[0], data.second.Ot.second.second[0],
              data.second.Ot.first[1], data.second.Ot.second.first[1],
              data.second.Ot.second.second[1], data.second.Ot.first[2],
              data.second.Ot.second.first[2], data.second.Ot.second.second[2];
          // Eigen::Matrix4f{data.second.Or * model_lrf.Or};

          Eigen::Matrix3f R_1{scene_lrf_Or.transpose() * model_lrf_Or};
          Eigen::Matrix3f R_2{scene_lrf_Ot.transpose() * model_lrf_Ot};

          Eigen::Vector4f t_1{};
          Eigen::Vector4f t_2{};
          Eigen::Vector3f m_1{model_lrf.r.x, model_lrf.r.y, model_lrf.r.z};
          Eigen::Vector3f m_2{model_lrf.t.x, model_lrf.t.y, model_lrf.t.z};

          m_1 = R_1 * m_1;
          m_2 = R_1 * m_2;

          t_1 << data.second.r.x - m_1[0], data.second.r.y - m_1[1],
              data.second.r.z - m_1[2], 1.0f;
          t_2 << data.second.t.x - m_2[0], data.second.t.y - m_2[1],
              data.second.t.z - m_2[2], 1.0f;
          // std::cout<<R_1<<std::endl;

          Eigen::Matrix4f T_1{};
          Eigen::Matrix4f T_2{};

          T_1 << R_1(0, 0), R_1(0, 1), R_1(0, 2), t_1[0], R_1(1, 0), R_1(1, 1),
              R_1(1, 2), t_1[1], R_1(2, 0), R_1(2, 1), R_1(2, 2), t_1[2], 0.0f,
              0.0f, 0.0f, t_1[3];
          T_2 << R_2(0, 0), R_2(0, 1), R_2(0, 2), t_2[0], R_2(1, 0), R_2(1, 1),
              R_2(1, 2), t_2[1], R_2(2, 0), R_2(2, 1), R_2(2, 2), t_2[2], 0.0f,
              0.0f, 0.0f, t_1[3];

          pcl::PointXYZ p;
          Eigen::Affine3f transform_1(T_1);
          Eigen::Affine3f transform_2(T_2);
          int index_1, index_2;
          index_1 = 0;
          index_2 = 0;
          for (int i = 0; i < 3; i++) {
            Eigen::Vector3f m{};
            Eigen::Vector3f s{};
            m << triple_set[i].x, triple_set[i].y, triple_set[i].z;
            s << 0.0f, 0.0f, 0.0f;
            pcl::transformPoint(m, s, transform_1);
            int xCell = static_cast<int>(
                            std::ceil((s[0] - this->x_range.first) /
                                      clustering_position_diff_threshold)) == 0
                            ? 1
                            : static_cast<int>(std::ceil(
                                  (s[0] - this->x_range.first) /
                                  clustering_position_diff_threshold));
            int yCell = static_cast<int>(
                            std::ceil((s[1] - this->y_range.first) /
                                      clustering_position_diff_threshold)) == 0
                            ? 1
                            : static_cast<int>(std::ceil(
                                  (s[1] - this->y_range.first) /
                                  clustering_position_diff_threshold));
            int zCell = static_cast<int>(
                            std::ceil((s[2] - this->z_range.first) /
                                      clustering_position_diff_threshold)) == 0
                            ? 1
                            : static_cast<int>(std::ceil(
                                  (s[2] - this->z_range.first) /
                                  clustering_position_diff_threshold));
            triple_scene->points.emplace_back(s[0], s[1], s[2]);
            index_1 +=
                (xCell - 1) + (yCell - 1) * x_num + (zCell - 1) * x_num * y_num;
            pcl::transformPoint(m, s, transform_2);
            xCell = static_cast<int>(
                        std::ceil((s[0] - this->x_range.first) /
                                  clustering_position_diff_threshold)) == 0
                        ? 1
                        : static_cast<int>(
                              std::ceil((s[0] - this->x_range.first) /
                                        clustering_position_diff_threshold));
            yCell = static_cast<int>(
                        std::ceil((s[1] - this->y_range.first) /
                                  clustering_position_diff_threshold)) == 0
                        ? 1
                        : static_cast<int>(
                              std::ceil((s[1] - this->y_range.first) /
                                        clustering_position_diff_threshold));
            zCell = static_cast<int>(
                        std::ceil((s[2] - this->z_range.first) /
                                  clustering_position_diff_threshold)) == 0
                        ? 1
                        : static_cast<int>(
                              std::ceil((s[2] - this->z_range.first) /
                                        clustering_position_diff_threshold));
            triple_scene->points.emplace_back(s[0], s[1], s[2]);
            index_2 +=
                (xCell - 1) + (yCell - 1) * x_num + (zCell - 1) * x_num * y_num;
          }
          this->vote(index_1, transform_1);
          this->vote(index_2, transform_2);
        } else {
          continue;
        }
      }
    }
  }

/*
  pcl::PointCloud<pcl::PointXYZ>::Ptr triple(
      new pcl::PointCloud<pcl::PointXYZ>());
  pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(
      new pcl::search::KdTree<pcl::PointXYZ>());
  tree->setInputCloud(triple_scene);

  std::vector<pcl::PointIndices> cluster_indices;
  pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
  ec.setClusterTolerance(this->clustering_position_diff_threshold);
  ec.setMinClusterSize(100);
  ec.setMaxClusterSize(25000);
  ec.setSearchMethod(tree);
  ec.setInputCloud(triple_scene);
  ec.extract(cluster_indices);
*/
  int final_key = -1;
  int max_vote = 0;
  for(auto i :this->map){
    if(i.second.value>max_vote){
      max_vote = i.second.value;
      final_key = i.first;
    }else{
      continue;
    }
  }
  std::cout<<"final vote: "<<max_vote<<std::endl;
  this->finalTransformation = map.find(final_key)->second.T;
  std::cout<<"transform matrix: "<<std::endl<<this->finalTransformation.matrix();
  /**generate cluster **/
  /*
  for (auto i = cluster_indices.begin(); i != cluster_indices.end(); ++i) {
    for (auto j = 0; j < i->indices.size(); j++) {
      triple->points.push_back(triple_scene->points[i->indices[j]]);
    }
  }
*/
/*visualize*/
/*
  pcl::visualization::PCLVisualizer view("subsampled point cloud");
  view.setBackgroundColor(0, 0, 0);
  pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> red(
      triple, 255, 0, 0);
  pcl::visualization::PointCloudColorHandlerCustom<pcl::PointNormal> white(
      scene_cloud_with_normal, 255, 255, 255);
  view.addPointCloud(triple, red, "triple");
  view.addPointCloud(scene_cloud_with_normal, white, "scene");
  while (!view.wasStopped()) {
    view.spinOnce(100);
    boost::this_thread::sleep(boost::posix_time::microseconds(1000));
  }
*/
}
