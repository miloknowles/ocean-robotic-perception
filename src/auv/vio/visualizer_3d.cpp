#include <chrono>

#include <glog/logging.h>
#include <eigen3/Eigen/Dense>
#include <opencv2/core/eigen.hpp>

#include "vio/visualizer_3d.hpp"

namespace bm {
namespace vio {


static std::string GetCameraPoseWidgetName(uid_t cam_id)
{
  return "cam_" + std::to_string(cam_id);
}


static std::string GetLandmarkWidgetName(uid_t lmk_id)
{
  return "lmk_" + std::to_string(lmk_id);
}


static cv::Affine3d EigenMatrix4dToCvAffine3d(const Matrix4d& T_world_cam)
{
  cv::Affine3d::Mat3 R_world_cam;
  cv::Affine3d::Vec3 t_world_cam;
  Eigen::Matrix3d _R_world_cam = T_world_cam.block<3, 3>(0, 0);
  Eigen::Vector3d _t_world_cam = T_world_cam.block<3, 1>(0, 3);
  cv::eigen2cv(_R_world_cam, R_world_cam);
  cv::eigen2cv(_t_world_cam, t_world_cam);
  return cv::Affine3d(R_world_cam, t_world_cam);
}


void Visualizer3D::AddCameraPose(uid_t cam_id, const Image1b& left_image, const Matrix4d& T_world_cam, bool is_keyframe)
{
  // cam_data_.emplace(cam_id, Visualizer3D::CameraData(cam_id, left_image, T_world_cam, is_keyframe));

  // TODO
  const cv::Matx33d K = {458.0, 0.0, left_image.cols / 2.0, 0.0, 458.0, left_image.rows / 2.0, 0.0, 0.0, 1.0};
  const cv::Affine3d T_world_cam_cv = EigenMatrix4dToCvAffine3d(T_world_cam);

  // REALTIME CAMERA: Show the current camera image inside a frustum.
  cv::viz::WCameraPosition widget_realtime(K, 1.0, cv::viz::Color::red());
  if (!left_image.empty()) {
    widget_realtime = cv::viz::WCameraPosition(K, left_image, 1.0, cv::viz::Color::red());
  }

  viz_lock_.lock();

  if (widget_names_.count("cam_realtime") != 0) {
    viz_.removeWidget("cam_realtime");
  }
  viz_.showWidget("cam_realtime", widget_realtime, T_world_cam_cv);
  widget_names_.insert("cam_realtime");

  // KEYFRAME CAMERA: If this is a keyframe, add a stereo camera frustum.
  if (is_keyframe) {
    const std::string widget_name = GetCameraPoseWidgetName(cam_id);
    cv::viz::WCameraPosition widget_keyframe(K, 1.0, cv::viz::Color::blue());
    viz_.showWidget(widget_name, widget_keyframe, T_world_cam_cv);
  }

  viz_lock_.unlock();
}


void Visualizer3D::UpdateCameraPose(uid_t cam_id, const Matrix4d& T_world_cam)
{
  // CHECK(cam_data_.count(cam_id) != 0) << "Tried to update camera pose that doesn't exist yet" << std::endl;
  // cam_data_.at(cam_id).T_world_cam = T_world_cam;

  const std::string widget_name = GetCameraPoseWidgetName(cam_id);
  CHECK(widget_names_.count(widget_name) != 0) << "Tried to update camera pose that doesn't exist yet" << std::endl;

  const cv::Affine3d T_world_cam_cv = EigenMatrix4dToCvAffine3d(T_world_cam);

  viz_lock_.lock();
  viz_.updateWidgetPose(widget_name, T_world_cam_cv);
  viz_lock_.unlock();
}


void Visualizer3D::AddOrUpdateLandmark(uid_t lmk_id, const Vector3d& t_world_lmk)
{
  lmk_data_.emplace(lmk_id, Visualizer3D::LandmarkData(lmk_id, t_world_lmk));
}


void Visualizer3D::AddLandmarkObservation(uid_t cam_id, uid_t lmk_id, const LandmarkObservation& lmk_obs)
{
  CHECK(lmk_data_.count(lmk_id) != 0) << "Tried to add observation for landmark that doesn't exist yet" << std::endl;
  lmk_data_.at(lmk_id).obs_cam_ids.emplace_back(cam_id);
}


void Visualizer3D::Start()
{
  // Set up visualizer window.
  viz_.showWidget("world_origin", cv::viz::WCameraPosition());
  viz_.setFullScreen(true);
  viz_.setBackgroundColor(cv::viz::Color::black());

  redraw_thread_ = std::thread(&Visualizer3D::RedrawThread, this);
  LOG(INFO) << "Starting RedrawThread ..." << std::endl;
}


void Visualizer3D::RedrawOnce()
{

}


void Visualizer3D::RedrawThread()
{
  while (!viz_.wasStopped()) {
    viz_lock_.lock();
    viz_.spinOnce(5, true);
    viz_lock_.unlock();

    // NOTE(milo): Need to sleep for a bit to let other functions get the mutex.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  LOG(INFO) << "Shutting down RedrawThread ..." << std::endl;
}


Visualizer3D::~Visualizer3D()
{
  redraw_thread_.join();
  LOG(INFO) << "Joined Visualizer3D redraw thread" << std::endl;
}


}
}
