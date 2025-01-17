//
// Created by yyh on 22-7-11.
//
#include "CentralVoting.h"
#include "PPFEstimation.h"
#include "PPFRegistration.h"
#include "SmartDownSample.h"
void CentralVoting::CenterExtractor(int index) {
  Eigen::Vector4f center;
  pcl::compute3DCentroid(*this->model_set[index], center);
  //std::cout << "pcl函数计算质心结果" << std::endl << center<<std::endl;
  pcl::PointXYZ p;
  p.x = center(0);
  p.y = center(1);
  p.z = center(2);

  pcl::MomentOfInertiaEstimation<pcl::PointXYZ> feature_extractor;
  feature_extractor.setInputCloud(this->model_set[index]);
  feature_extractor.compute();

  pcl::PointXYZ min_point_AABB;
  pcl::PointXYZ max_point_AABB;
  feature_extractor.getAABB(min_point_AABB, max_point_AABB);

  pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> model_color(
      255, 255, 255);
  pcl::PointXYZ p_faux(center[0], center[1], center[2]);
  pcl::PointXYZ p_saux(center[0], center[1], center[2]);
  pcl::PointXYZ c(center[0], center[1], center[2]);
  double d_obj = std::sqrt(std::pow(max_point_AABB.x - min_point_AABB.x, 2) +
                           std::pow(max_point_AABB.y - min_point_AABB.y, 2) +
                           std::pow(max_point_AABB.z - min_point_AABB.z, 2));
  this->d_obj_set.push_back(static_cast<float>(d_obj));
  std::cout << "\n模型体半径: " << d_obj << std::endl;
  p_faux.x -= static_cast<float>(d_obj);
  p_saux.y -= static_cast<float>(d_obj);

  this->triple_set[index].push_back(c);
  this->triple_set[index].push_back(p_faux);
  this->triple_set[index].push_back(p_saux);

/*
  pcl::visualization::PCLVisualizer view("model with center point");
    pcl::PointCloud<pcl::PointXYZ>::Ptr triple_cloud(
        new pcl::PointCloud<pcl::PointXYZ>());
    triple_cloud->points.push_back(c);
    triple_cloud->points.push_back(p_faux);
    triple_cloud->points.push_back(p_saux);

    // visualize
    view.addPointCloud(this->model_set[index], model_color, "model");

    view.setBackgroundColor(0, 0, 0);

    view.addCube(min_point_AABB.x, max_point_AABB.x, min_point_AABB.y,
                 max_point_AABB.y, min_point_AABB.z, max_point_AABB.z, 1.0, 1.0,
                 0.0, "AABB");
    view.setShapeRenderingProperties(
        pcl::visualization::PCL_VISUALIZER_REPRESENTATION,
        pcl::visualization::PCL_VISUALIZER_REPRESENTATION_WIREFRAME, "AABB");
    pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> red(
        triple_cloud, 255, 0, 0);
    view.addPointCloud(triple_cloud, red, "triple");
    view.setPointCloudRenderingProperties(
        pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 10, "triple");

    while (!view.wasStopped()) {
      view.spinOnce(100);
      boost::this_thread::sleep(boost::posix_time::microseconds(1000));
    }
    */
}

pcl::PointCloud<pcl::PointNormal>::Ptr CentralVoting::DownSample(
    const pcl::PointCloud<pcl::PointXYZ>::Ptr &input_cloud) const {
  pcl::PointXYZ max_point, min_point;
  GenerateBound(input_cloud, max_point, min_point);
  SmartDownSample sample_filter(input_cloud,
                                std::make_pair(min_point.x, max_point.x),
                                std::make_pair(min_point.y, max_point.y),
                                std::make_pair(min_point.z, max_point.z),
                                this->step, this->AngleThreshold, 0.01);
  sample_filter.setIsdense(false);
  sample_filter.setKSearch(this->k_point);
  return sample_filter.compute();
}
pcl::PointCloud<pcl::PointNormal>::Ptr CentralVoting::DownSample(
    const pcl::PointCloud<pcl::PointXYZ>::Ptr &input_cloud,
    const std::vector<pcl::PointXYZ> &view_point, const bool &reverse) const {
  pcl::PointXYZ max_point, min_point;
  GenerateBound(input_cloud, max_point, min_point);
  SmartDownSample sample_filter(input_cloud,
                                std::make_pair(min_point.x, max_point.x),
                                std::make_pair(min_point.y, max_point.y),
                                std::make_pair(min_point.z, max_point.z),
                                this->step, this->AngleThreshold, 0.01);
  sample_filter.setIsdense(false);
  sample_filter.setViewPoint(Eigen::Vector3f(view_point[0].x, view_point[0].y, view_point[0].z));
  sample_filter.setReverse(true);
  sample_filter.setKSearch(this->k_point);
  return sample_filter.compute();
}
std::vector<Eigen::Affine3f> CentralVoting::Solve() {
    std::cout << "scene降采样开始： " << std::endl;
    this->scene_subsampled = DownSample(scene);
   //this->scene_subsampled = subsampleAndCalculateNormals(scene);
  //Eigen::Vector4f center;
  //pcl::compute3DCentroid(*scene, center);
  // this->scene_subsampled = subsampleAndCalculateNormals(scene, center[0]+200,
  // center[1], center[2], false);
  //this->scene_subsampled = subsampleAndCalculateNormals(
      //scene, Eigen::Vector4f(8.0f, 8.0f, 8.0f, 0.0f));
  std::vector<pcl::PointCloud<pcl::PointNormal>::Ptr> cloud_models_with_normal;
  std::vector<PPF::searchMapType> hashmap_search_vector;
  std::cout << "model降采样开始： " << std::endl;
  std::cout << "model数量："<<this->model_set.size()<<std::endl;
  for (auto i = 0; i < this->model_set.size(); i++) {
    //auto model_cloud = SimpleDownSample(model_set[i]);
    pcl::PointCloud<pcl::PointNormal>::Ptr model_with_normal =
    DownSample(model_set[i], this->triple_set[i], true);
     //pcl::PointCloud<pcl::PointNormal>::Ptr model_with_normal =
     //subsampleAndCalculateNormals(model_set[i]);
    //pcl::PointCloud<pcl::PointNormal>::Ptr model_with_normal =
       // subsampleAndCalculateNormals(model_set[i], this->triple_set[i], true);
    cloud_models_with_normal.push_back(model_with_normal);
    /**
     * 可视化法线
     *
     *
     *

        pcl::visualization::PCLVisualizer view("subsampled point cloud");
        view.setBackgroundColor(0, 0, 0);
        pcl::visualization::PointCloudColorHandlerCustom<pcl::PointNormal> red(
            model_with_normal, 255, 0, 0);
        view.addPointCloud(model_with_normal, red, "model");
        view.addPointCloudNormals<pcl::PointNormal>(model_with_normal, 1, 5,
                                                    "model with normal");

        pcl::visualization::PointCloudColorHandlerCustom<pcl::PointNormal>
     white( scene_subsampled, 0, 255, 0); view.addPointCloud(scene_subsampled,
     white, "scene");
        view.addPointCloudNormals<pcl::PointNormal>(scene_subsampled, 1, 5,
     "scene with normals"); while (!view.wasStopped()) { view.spinOnce(100);
          boost::this_thread::sleep(boost::posix_time::microseconds(1000));
        }
       **/
    PPFEstimation ppf_estimator;
    ppf_estimator.setDiscretizationSteps(6.0f / 180.0f * float(M_PI), 0.05f);
    ppf_estimator.setDobj(this->d_obj_set[i]);
    // start = clock();
    int Nd = std::floor(this->d_obj_set[i]/ 0.05f) + 1;
    int Na = std::floor(float(M_PI) / (6.0f / 180.0f * float(M_PI))) + 1;

    PPF::searchMapType
        PPF_map_(Nd,
            std::vector<std::vector<std::vector<std::vector<Hash::HashData>>>>(
                Na,
                std::vector<std::vector<std::vector<Hash::HashData>>>(
                    Na, std::vector<std::vector<Hash::HashData>>(
                            Na, std::vector<Hash::HashData>(
                                    0)))));  //产生静态数组


    ppf_estimator.compute(model_with_normal,PPF_map_);

    hashmap_search_vector.push_back(PPF_map_);
  }

//  pcl::visualization::PCLVisualizer view("registration result");
//  view.setBackgroundColor(0, 0, 0);
  PCL_INFO("registration阶段开始\n");

  Eigen::Matrix4f GT{};
  std::vector<Eigen::Affine3f> results;
  GT<<0.999126, 0.0369223, 0.0196902, -100.672,
            -0.0372036, 0.999209, 0.0140794, 171.854,
            -0.0191553, -0.014799, 0.999706, -16.0715,
            0, 0, 0, 1;
            auto tp1 = std::chrono::steady_clock::now();
  for (std::size_t model_i = 0; model_i < model_set.size(); ++model_i) {
    PPFRegistration ppf_registration{};
    ppf_registration.setSceneReferencePointSamplingRate(10);
    ppf_registration.setPositionClusteringThreshold(0.5);  //投票的体素网格的size
    ppf_registration.setRotationClusteringThreshold(6.0f / 180.0f *
                                                    float(M_PI));
    ppf_registration.setSearchMap(hashmap_search_vector[model_i]);
    ppf_registration.setInputSource(cloud_models_with_normal[model_i]);
    ppf_registration.setInputTarget(this->scene_subsampled);
    ppf_registration.setModelTripleSet(this->triple_set[model_i]);
    ppf_registration.setDobj(this->d_obj_set[model_i]);
    ppf_registration.setDiscretizationSteps(6.0f / 180.0f * float(M_PI),
                                            0.05f);
    ppf_registration.setGroundTruthTransform(GT);
    tp1 = std::chrono::steady_clock::now();

    results = ppf_registration.compute();
    PCL_INFO("registration阶段完成\n");
    Eigen::Affine3f T = ppf_registration.getFinalTransformation();
//    pcl::PointCloud<pcl::PointXYZ>::Ptr output_model(
//        new pcl::PointCloud<pcl::PointXYZ>());
//    pcl::transformPointCloud(*this->model_set[model_i], *output_model, T);
//
//    pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> red(
//        output_model, 255, 0, 0);
//    pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> white(
//        this->scene, 255, 255, 255);
//    pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> s(
//        this->model_set[model_i], 0, 255, 0);
//    // view.addPointCloud(model_set[model_i], s, "model");
//    view.addPointCloud(output_model, red, "out");
//    view.addPointCloud(this->scene, white, "scene");
  }
  auto tp2 = std::chrono::steady_clock::now();
  std::cout << "\nneed "
            << std::chrono::duration_cast<std::chrono::milliseconds>(tp2 - tp1)
                   .count()
            << "ms for online process" << std::endl;
//  while (!view.wasStopped()) {
//    view.spinOnce(100);
//    boost::this_thread::sleep(boost::posix_time::microseconds(1000));
//  }

  return results;
}

void CentralVoting::test() {
  /*
  pcl::PointCloud<pcl::PointXYZ>::Ptr scene_ =
      boost::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  if (isAdaptiveDownSample) {
    scene_ = adaptiveDownSample(this->scene);
  } else {
    scene_ = SimpleDownSample(this->scene);
  }
*/
  auto model_with_normal = DownSample(model_set[0]);
  pcl::visualization::PCLVisualizer view("subsampled point cloud");
  view.setBackgroundColor(0, 0, 0);

  pcl::visualization::PointCloudColorHandlerCustom<pcl::PointNormal> red(
      model_with_normal, 255, 0, 0);
  view.addPointCloud(model_with_normal, red, "cloud");
  view.addPointCloudNormals<pcl::PointNormal>(model_with_normal, 10, 0.5,
                                              "cloud with normal");

  /*
   pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> red(
      model_with_normal, 255, 0, 0);
  view.addPointCloud(model_with_normal, red, "cloud");
*/
  while (!view.wasStopped()) {
    view.spinOnce(100);
    boost::this_thread::sleep(boost::posix_time::microseconds(1000));
  }
}
void CentralVoting::GenerateBound(
    const pcl::PointCloud<pcl::PointXYZ>::Ptr &input_cloud,
    pcl::PointXYZ &max_point, pcl::PointXYZ &min_point) {
  pcl::MomentOfInertiaEstimation<pcl::PointXYZ> feature_extractor;
  feature_extractor.setInputCloud(input_cloud);
  feature_extractor.compute();
  feature_extractor.getAABB(min_point, max_point);
}

bool CentralVoting::CenterExtractorAll() {
  if (this->model_set.empty()) {
    PCL_ERROR("there is no model point cloud in the model set\n");
    return false;
  } else {
    this->InitTripleSet();
    for (auto i = 0; i < this->model_set.size(); i++) {
      CenterExtractor(i);
    }
    PCL_INFO("All models has finished triple set extraction\n");
    return true;
  }
}

void CentralVoting::InitTripleSet() {
  this->triple_set.resize(this->model_set.size());
}

void CentralVoting::setAngleThreshold(const float &angle) {
  this->AngleThreshold = angle;
}
void CentralVoting::setDownSampleStep(const float &step) { this->step = step; }
void CentralVoting::setNormalEstimationRadius(const float &radius) {
  this->normalEstimationRadius = radius;
}

bool CentralVoting::AddModel(pcl::PointCloud<pcl::PointXYZ>::Ptr input_model) {
  if (this->model_set.size() > maxModelNum) {
    PCL_ERROR("model vector is full");
    return false;
  } else {
    this->model_set.push_back(std::move(input_model));
    return true;
  }
}
void CentralVoting::setSimpleDownSampleLeaf(
    const Eigen::Vector4f &subsampling_leaf_size) {
  this->subsampling_leaf_size = subsampling_leaf_size;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr CentralVoting::SimpleDownSample(
    pcl::PointCloud<pcl::PointXYZ>::Ptr &input_cloud) {
  std::cout << "input_cloud_size:" << input_cloud->points.size() << std::endl;
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_subsampled(
      new pcl::PointCloud<pcl::PointXYZ>());
  pcl::VoxelGrid<pcl::PointXYZ> subsampling_filter;
  subsampling_filter.setInputCloud(input_cloud);
  subsampling_filter.setLeafSize(this->subsampling_leaf_size);
  subsampling_filter.filter(*cloud_subsampled);
  std::cout << "output_cloud_size:" << cloud_subsampled->points.size()
            << std::endl;
  return cloud_subsampled;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr CentralVoting::adaptiveDownSample(
    const pcl::PointCloud<pcl::PointXYZ>::Ptr input_cloud) {
  std::cout << "input_cloud_size:" << input_cloud->points.size() << std::endl;
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_subsampled(
      new pcl::PointCloud<pcl::PointXYZ>());
  pcl::VoxelGrid<pcl::PointXYZ> subsampling_filter;
  subsampling_filter.setInputCloud(input_cloud);
  Eigen::Vector4f leaf_size;

  if (this->adaptive_step != 0) {
    leaf_size << adaptive_step, adaptive_step, adaptive_step, 0.0f;
  } else {
    pcl::PointXYZ min_p, max_p;
    GenerateBound(input_cloud, max_p, min_p);
    float max_l = std::abs(static_cast<float>(std::max(
        max_p.x - min_p.x, std::max(max_p.y - min_p.y, max_p.z - min_p.z))));
    float s = std::ceil(max_l / pow(this->downSampleTarget, (0.5)));
    std::cout << "max_l: " << max_l << std::endl;
    std::cout << "adaptive step: " << s << std::endl;
    leaf_size << s, s, s, 0.0f;
  }

  subsampling_filter.setLeafSize(leaf_size);
  subsampling_filter.filter(*cloud_subsampled);
  std::cout << "output_cloud_size:" << cloud_subsampled->points.size()
            << std::endl;
  return cloud_subsampled;
}

void CentralVoting::setAdaptiveDownSampleOption(const bool &lhs, const int &rhs,
                                                const float &step_) {
  this->isAdaptiveDownSample = lhs;
  this->downSampleTarget = rhs;
  if (step_ != 0) {
    this->adaptive_step = step_;
  }
}

pcl::PointCloud<pcl::PointNormal>::Ptr
CentralVoting::subsampleAndCalculateNormals(
    const pcl::PointCloud<pcl::PointXYZ>::Ptr &cloud)  //降采样并计算表面法向量
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_subsampled(
      new pcl::PointCloud<
          pcl::PointXYZ>());  //直接进行降采样，没有进行额外的处理
  pcl::VoxelGrid<pcl::PointXYZ> subsampling_filter;  //创建体素栅格
  subsampling_filter.setInputCloud(cloud);
  subsampling_filter.setLeafSize(subsampling_leaf_size);  // 设置采样体素大小
  subsampling_filter.filter(*cloud_subsampled);

  pcl::PointCloud<pcl::Normal>::Ptr cloud_subsampled_normals(
      new pcl::PointCloud<pcl::Normal>());
  pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimation_filter;
  normal_estimation_filter.setInputCloud(cloud_subsampled);
  pcl::search::KdTree<pcl::PointXYZ>::Ptr search_tree(
      new pcl::search::KdTree<pcl::PointXYZ>);  ////建立kdtree来进行近邻点集搜索
  normal_estimation_filter.setSearchMethod(search_tree);
  normal_estimation_filter.setKSearch(k_point);
  // normal_estimation_filter.setRadiusSearch(normalEstimationRadius);
  normal_estimation_filter.compute(*cloud_subsampled_normals);

  pcl::PointCloud<pcl::PointNormal>::Ptr cloud_subsampled_with_normals(
      new pcl::PointCloud<pcl::PointNormal>());
  concatenateFields(
      *cloud_subsampled, *cloud_subsampled_normals,
      *cloud_subsampled_with_normals);  // concatenate point cloud and its
                                        // normal into a new cloud

  PCL_INFO("Cloud dimensions before / after subsampling: %zu / %zu\n",
           static_cast<std::size_t>(cloud->size()),
           static_cast<std::size_t>(cloud_subsampled->size()));
  return cloud_subsampled_with_normals;
}

pcl::PointCloud<pcl::PointNormal>::Ptr
CentralVoting::subsampleAndCalculateNormals(
    const pcl::PointCloud<pcl::PointXYZ>::Ptr &cloud,
    const Eigen::Vector4f &leaf_size) const  //降采样并计算表面法向量
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_subsampled(
      new pcl::PointCloud<
          pcl::PointXYZ>());  //直接进行降采样，没有进行额外的处理
  pcl::VoxelGrid<pcl::PointXYZ> subsampling_filter;  //创建体素栅格
  subsampling_filter.setInputCloud(cloud);
  subsampling_filter.setLeafSize(leaf_size);  // 设置采样体素大小
  subsampling_filter.filter(*cloud_subsampled);

  pcl::PointCloud<pcl::Normal>::Ptr cloud_subsampled_normals(
      new pcl::PointCloud<pcl::Normal>());
  pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimation_filter;
  normal_estimation_filter.setInputCloud(cloud_subsampled);
  pcl::search::KdTree<pcl::PointXYZ>::Ptr search_tree(
      new pcl::search::KdTree<pcl::PointXYZ>);  ////建立kdtree来进行近邻点集搜索
  normal_estimation_filter.setSearchMethod(search_tree);
  normal_estimation_filter.setKSearch(k_point);
  // normal_estimation_filter.setRadiusSearch(normalEstimationRadius);
  normal_estimation_filter.compute(*cloud_subsampled_normals);

  pcl::PointCloud<pcl::PointNormal>::Ptr cloud_subsampled_with_normals(
      new pcl::PointCloud<pcl::PointNormal>());
  concatenateFields(
      *cloud_subsampled, *cloud_subsampled_normals,
      *cloud_subsampled_with_normals);  // concatenate point cloud and its
                                        // normal into a new cloud

  PCL_INFO("Cloud dimensions before / after subsampling: %zu / %zu\n",
           static_cast<std::size_t>(cloud->size()),
           static_cast<std::size_t>(cloud_subsampled->size()));
  return cloud_subsampled_with_normals;
}

pcl::PointCloud<pcl::PointNormal>::Ptr
CentralVoting::subsampleAndCalculateNormals(
    const pcl::PointCloud<pcl::PointXYZ>::Ptr &cloud,
    const std::vector<pcl::PointXYZ> &view_point, const bool &reverse) {
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_subsampled(
      new pcl::PointCloud<
          pcl::PointXYZ>());  //直接进行降采样，没有进行额外的处理
  pcl::VoxelGrid<pcl::PointXYZ> subsampling_filter;  //创建体素栅格
  subsampling_filter.setInputCloud(cloud);
  subsampling_filter.setLeafSize(subsampling_leaf_size);  // 设置采样体素大小
  subsampling_filter.filter(*cloud_subsampled);

  pcl::PointCloud<pcl::Normal>::Ptr cloud_subsampled_normals(
      new pcl::PointCloud<pcl::Normal>());
  pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimation_filter;
  normal_estimation_filter.setViewPoint(view_point[0].x, view_point[0].y,
                                        view_point[0].z);
  normal_estimation_filter.setInputCloud(cloud_subsampled);
  pcl::search::KdTree<pcl::PointXYZ>::Ptr search_tree(
      new pcl::search::KdTree<pcl::PointXYZ>);  ////建立kdtree来进行近邻点集搜索
  normal_estimation_filter.setSearchMethod(search_tree);
  normal_estimation_filter.setKSearch(k_point);
  // normal_estimation_filter.setRadiusSearch(normalEstimationRadius);
  normal_estimation_filter.compute(*cloud_subsampled_normals);
  if (reverse) {
    for (auto &i : *cloud_subsampled_normals) {
      i.normal_x = -i.normal_x;
      i.normal_y = -i.normal_y;
      i.normal_z = -i.normal_z;
      i.normal[0] = i.normal_x;
      i.normal[1] = i.normal_y;
      i.normal[2] = i.normal_z;
      i.curvature = -i.curvature;
    }
  }
  pcl::PointCloud<pcl::PointNormal>::Ptr cloud_subsampled_with_normals(
      new pcl::PointCloud<pcl::PointNormal>());
  concatenateFields(
      *cloud_subsampled, *cloud_subsampled_normals,
      *cloud_subsampled_with_normals);  // concatenate point cloud and its
                                        // normal into a new cloud

  PCL_INFO("Cloud dimensions before / after subsampling: %zu / %zu\n",
           static_cast<std::size_t>(cloud->size()),
           static_cast<std::size_t>(cloud_subsampled->size()));
  return cloud_subsampled_with_normals;
}

pcl::PointCloud<pcl::PointNormal>::Ptr
CentralVoting::subsampleAndCalculateNormals(
    const pcl::PointCloud<pcl::PointXYZ>::Ptr &cloud, const float x,
    const float y, const float z, const bool &reverse) {
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_subsampled(
      new pcl::PointCloud<
          pcl::PointXYZ>());  //直接进行降采样，没有进行额外的处理
  pcl::VoxelGrid<pcl::PointXYZ> subsampling_filter;  //创建体素栅格
  subsampling_filter.setInputCloud(cloud);
  subsampling_filter.setLeafSize(subsampling_leaf_size);  // 设置采样体素大小
  subsampling_filter.filter(*cloud_subsampled);

  pcl::PointCloud<pcl::Normal>::Ptr cloud_subsampled_normals(
      new pcl::PointCloud<pcl::Normal>());
  pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimation_filter;
  normal_estimation_filter.setViewPoint(x, y, z);
  normal_estimation_filter.setInputCloud(cloud_subsampled);
  pcl::search::KdTree<pcl::PointXYZ>::Ptr search_tree(
      new pcl::search::KdTree<pcl::PointXYZ>);  ////建立kdtree来进行近邻点集搜索
  normal_estimation_filter.setSearchMethod(search_tree);
  normal_estimation_filter.setKSearch(k_point);
  // normal_estimation_filter.setRadiusSearch(normalEstimationRadius);
  normal_estimation_filter.compute(*cloud_subsampled_normals);
  if (reverse) {
    for (auto &i : *cloud_subsampled_normals) {
      i.normal_x = -i.normal_x;
      i.normal_y = -i.normal_y;
      i.normal_z = -i.normal_z;
      i.normal[0] = i.normal_x;
      i.normal[1] = i.normal_y;
      i.normal[2] = i.normal_z;
      i.curvature = -i.curvature;
    }
  }
  pcl::PointCloud<pcl::PointNormal>::Ptr cloud_subsampled_with_normals(
      new pcl::PointCloud<pcl::PointNormal>());
  concatenateFields(
      *cloud_subsampled, *cloud_subsampled_normals,
      *cloud_subsampled_with_normals);  // concatenate point cloud and its
                                        // normal into a new cloud

  PCL_INFO("Cloud dimensions before / after subsampling: %zu / %zu\n",
           static_cast<std::size_t>(cloud->size()),
           static_cast<std::size_t>(cloud_subsampled->size()));
  return cloud_subsampled_with_normals;
}

