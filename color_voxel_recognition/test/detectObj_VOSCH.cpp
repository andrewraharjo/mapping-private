#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <float.h>
#include <color_voxel_recognition/glView.hpp>
#include <color_voxel_recognition/Voxel.hpp>
#include <color_voxel_recognition/Param.hpp>
#include <color_voxel_recognition/libPCA_eigen.hpp>
#include <color_voxel_recognition/Param.hpp>
//#include <color_voxel_recognition/CCHLAC.hpp>
#include <color_voxel_recognition/Search_VOSCH.hpp>
#include "color_chlac/grsd_colorCHLAC_tools.h"
#include "../param/FILE_MODE"
#include "../param/CAM_SIZE"
#include <ros/ros.h>
#include "pcl/io/pcd_io.h"
#include "pcl_ros/subscriber.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

/****************************************************************************************************************************/
/* 物体検出を行い、類似度が閾値以上の領域を表示する                                                                                  */
/*  ./detectObj <rank_num> <exist_voxel_num_threshold> [model_pca_filename] <dim_model> <size1> <size2> <size3> <detect_th> */
/*  例 ./detectObj 10 30 models/phone1/pca_result 40 0.1 0.1 0.1 20                                                         */
/*  スペースキーかEnterキーを押すと、attentionモード（検出領域内のみを表示）と全体表示モードが切り替わる                                     */
/*  注） プレビューはMESH_VIEWがtrueならメッシュ、falseならボクセルで表示。                                                           */
/****************************************************************************************************************************/

using namespace std;
using namespace Eigen3;

//float DISTANCE_TH = 1.0;
float distance_th;

//************************
//* その他のグローバル変数など
//Voxel voxel;

SearchObjVOSCH search_obj;
int color_threshold_r, color_threshold_g, color_threshold_b;
int dim;
int box_size;
float voxel_size;
float region_size;
float sliding_box_size;
bool exit_flg = false;
bool start_flg = true;
float detect_th = 0;
int rank_num;

//* Note that x and y values are inverted.
template <typename T>
int limitPoint( const pcl::PointCloud<T> input_cloud, pcl::PointCloud<T> &output_cloud, const float dis_th ){
  output_cloud.width = input_cloud.width;
  output_cloud.height = input_cloud.height;
  const int v_num = input_cloud.points.size();
  output_cloud.points.resize( v_num );

  int idx = 0;
  for( int i=0; i<v_num; i++ ){
    if( input_cloud.points[ i ].z < dis_th ){
      output_cloud.points[ idx ] = input_cloud.points[ i ];
      //output_cloud.points[ idx ].x = - input_cloud.points[ i ].x; // inverted.
      //output_cloud.points[ idx ].y = - input_cloud.points[ i ].y; // inverted.
      idx++;
    }
  }
  output_cloud.width = idx;
  output_cloud.height = 1;
  output_cloud.points.resize( idx );
  cout << "from " << input_cloud.points.size() << " to " << idx << " points" << endl;
  return idx;
}

/////////////////////////////////////////////
//* ボクセル化、データの保存のためのクラス
class ViewAndDetect {
protected:
  ros::NodeHandle nh_;
private:
  pcl::PointCloud<pcl::PointXYZRGB> cloud_xyzrgb_, cloud_xyzrgb;
  pcl::PointCloud<pcl::PointXYZRGBNormal> cloud_normal, cloud_downsampled;
  pcl::VoxelGrid<pcl::PointXYZRGBNormal> grid;
  bool relative_mode; // RELATIVE MODEにするか否か
  double t1, t2, t0;
public:
  void activateRelativeMode(){ relative_mode = true; }
    string cloud_topic_;
    pcl_ros::Subscriber<sensor_msgs::PointCloud2> sub_;
    ros::Publisher marker_pub_;
    ros::Publisher marker_array_pub_;
  //***************
  //* コンストラクタ
  ViewAndDetect() :
    relative_mode(false) {
  }

  //*******************************
  //* ボクセル化、データの保存
  void vad_cb(const sensor_msgs::PointCloud2ConstPtr& cloud) {
      if ((cloud->width * cloud->height) == 0)
        return;
      //ROS_INFO ("Received %d data points in frame %s with the following fields: %s", (int)cloud->width * cloud->height, cloud->header.frame_id.c_str (), pcl::getFieldsList (*cloud).c_str ());
      //cout << "fromROSMsg?" << endl;
      pcl::fromROSMsg (*cloud, cloud_xyzrgb_);
      //cout << "  fromROSMsg done." << endl;
      cout << "CREATE" << endl;
      t0 = my_clock();

      // float x_min = 10000000, y_min = 10000000, z_min = 10000000;
      // float x_max = -10000000, y_max = -10000000, z_max = -10000000;
      // int pnum = cloud_xyzrgb_.points.size();
      // for( int p=0; p<pnum; p++ ){
      // 	if( cloud_xyzrgb_.points[ p ].x < x_min ) x_min = cloud_xyzrgb_.points[ p ].x;
      // 	if( cloud_xyzrgb_.points[ p ].y < y_min ) y_min = cloud_xyzrgb_.points[ p ].y;
      // 	if( cloud_xyzrgb_.points[ p ].z < z_min ) z_min = cloud_xyzrgb_.points[ p ].z;
      // 	if( cloud_xyzrgb_.points[ p ].x > x_max ) x_max = cloud_xyzrgb_.points[ p ].x;
      // 	if( cloud_xyzrgb_.points[ p ].y > y_max ) y_max = cloud_xyzrgb_.points[ p ].y;
      // 	if( cloud_xyzrgb_.points[ p ].z > z_max ) z_max = cloud_xyzrgb_.points[ p ].z;
      // }
      //cout << x_min << " " << y_min << " " << z_min << endl;
      //cout << x_max << " " << y_max << " " << z_max << endl;

      //* limit distance
      //* Note that x and y values are inverted.
      // if( relative_mode ){
      // 	float dis_min = FLT_MAX;
      // 	for (int i=0; i<(int)cloud_xyzrgb_.points.size(); i++)
      // 	  if( dis_min > cloud_xyzrgb_.points[ i ].z ) dis_min = cloud_xyzrgb_.points[ i ].z;
      // 	limitPoint( cloud_xyzrgb_, cloud_xyzrgb, dis_min + distance_th );
      // }
      // else
      //	limitPoint( cloud_xyzrgb_, cloud_xyzrgb, distance_th );
      if( limitPoint( cloud_xyzrgb_, cloud_xyzrgb, distance_th ) > 10 ){
	//cout << "  limit done." << endl;
	cout << "compute normals and voxelize...." << endl;
	
	//****************************************
	//* compute normals
	computeNormal( cloud_xyzrgb, cloud_normal );
	
	//* voxelize
	getVoxelGrid( grid, cloud_normal, cloud_downsampled, voxel_size );
	//voxel.points2voxel( cloud_downsampled, SIMPLE_REVERSE );
	cout << "     ...done.." << endl;
	
	const int pnum = cloud_downsampled.points.size();
	float x_min = 10000000, y_min = 10000000, z_min = 10000000;
	float x_max = -10000000, y_max = -10000000, z_max = -10000000;
	for( int p=0; p<pnum; p++ ){
	  if( cloud_downsampled.points[ p ].x < x_min ) x_min = cloud_downsampled.points[ p ].x;
	  if( cloud_downsampled.points[ p ].y < y_min ) y_min = cloud_downsampled.points[ p ].y;
	  if( cloud_downsampled.points[ p ].z < z_min ) z_min = cloud_downsampled.points[ p ].z;
	  if( cloud_downsampled.points[ p ].x > x_max ) x_max = cloud_downsampled.points[ p ].x;
	  if( cloud_downsampled.points[ p ].y > y_max ) y_max = cloud_downsampled.points[ p ].y;
	  if( cloud_downsampled.points[ p ].z > z_max ) z_max = cloud_downsampled.points[ p ].z;
	}
	//cout << x_min << " " << y_min << " " << z_min << endl;
	//cout << x_max << " " << y_max << " " << z_max << endl;
	//cout << grid.getMinBoxCoordinates() << endl;

	cout << "search start..." << endl;
	t1 = my_clock();
	//****************************************
	//* 物体検出
	search_obj.cleanData();
	search_obj.setData( dim, color_threshold_r, color_threshold_g, color_threshold_b, grid, cloud_normal, cloud_downsampled, voxel_size, box_size, false );
	if( ( search_obj.XYnum() != 0 ) && ( search_obj.Znum() != 0 ) )
	  search_obj.search();
	
	//* 物体検出 ここまで
	//****************************************
	t2 = my_clock();
	cout << "  ...search done." << endl;
	
	// if( ( voxel.Xsize() != 0 ) && ( voxel.Ysize() != 0 ) && ( voxel.Zsize() != 0 ) )
	// 	v_state = DISP;
	cout << "voxelize                   :"<< t1 - t0 << " sec" << endl;
	cout << "feature extraction & search: "<< t2 - t1 << " sec" <<endl;
	cout << "all processes              : "<< t2 - t0 << " sec" << endl;
	marker_pub_ = nh_.advertise<visualization_msgs::Marker>("visualization_marker", 1); 
	marker_array_pub_ = nh_.advertise<visualization_msgs::MarkerArray>("visualization_marker_array", 100);
	visualization_msgs::MarkerArray marker_array_msg_;
	
	//* show the limited space
	visualization_msgs::Marker marker_;
	marker_.header.frame_id = "openni_depth_optical_frame";
	marker_.header.stamp = ros::Time::now();
	marker_.ns = "BoxEstimation";
	marker_.id = -1;
	marker_.type = visualization_msgs::Marker::CUBE;
	marker_.action = visualization_msgs::Marker::ADD;
	marker_.pose.position.x = (x_max+x_min)/2;
	marker_.pose.position.y = (y_max+y_min)/2;
	marker_.pose.position.z = (z_max+z_min)/2;
	marker_.pose.orientation.x = 0;
	marker_.pose.orientation.y = 0;
	marker_.pose.orientation.z = 0;
	marker_.pose.orientation.w = 1;
	marker_.scale.x = x_max-x_min;
	marker_.scale.y = x_max-x_min;
	marker_.scale.z = x_max-x_min;
	marker_.color.a = 0.1;
	marker_.color.r = 1.0;
	marker_.color.g = 0.0;
	marker_.color.b = 0.0;
	marker_.lifetime = ros::Duration();
	// std::cerr << "BOX MARKER COMPUTED, WITH FRAME " << marker_.header.frame_id << " POSITION: " 
	// 	  << marker_.pose.position.x << " " << marker_.pose.position.y << " " 
	// 	  << marker_.pose.position.z << std::endl;
	marker_array_msg_.markers.push_back(marker_);
	
	for( int q=0; q<rank_num; q++ ){
	  if( search_obj.maxDot( q ) < detect_th ) break;
	  cout << search_obj.maxX( q ) << " " << search_obj.maxY( q ) << " " << search_obj.maxZ( q ) << endl;
	  cout << "dot " << search_obj.maxDot( q ) << endl;
	  //if( (search_obj.maxX( q )!=0)||(search_obj.maxY( q )!=0)||(search_obj.maxZ( q )!=0) ){
	  //* publish marker
	  visualization_msgs::Marker marker_;
	  //marker_.header.frame_id = "base_link";
	  marker_.header.frame_id = "openni_depth_optical_frame";
	  marker_.header.stamp = ros::Time::now();
	  marker_.ns = "BoxEstimation";
	  marker_.id = q;
	  marker_.type = visualization_msgs::Marker::CUBE;
	  marker_.action = visualization_msgs::Marker::ADD;
	  marker_.pose.position.x = search_obj.maxX( q ) * region_size + sliding_box_size/2 + x_min;
	  marker_.pose.position.y = search_obj.maxY( q ) * region_size + sliding_box_size/2 + y_min;
	  marker_.pose.position.z = search_obj.maxZ( q ) * region_size + sliding_box_size/2 + z_min;
	  marker_.pose.orientation.x = 0;
	  marker_.pose.orientation.y = 0;
	  marker_.pose.orientation.z = 0;
	  marker_.pose.orientation.w = 1;
	  marker_.scale.x = sliding_box_size;
	  marker_.scale.y = sliding_box_size;
	  marker_.scale.z = sliding_box_size;
	  marker_.color.a = 0.5;
	  marker_.color.r = 0.0;
	  marker_.color.g = 1.0;
	  marker_.color.b = 0.0;
	  marker_.lifetime = ros::Duration();
	  // std::cerr << "BOX MARKER COMPUTED, WITH FRAME " << marker_.header.frame_id << " POSITION: " 
	  // 	    << marker_.pose.position.x << " " << marker_.pose.position.y << " " 
	  // 	    << marker_.pose.position.z << std::endl;
	  //   marker_pub_.publish (marker_);    
	  // }
	  marker_array_msg_.markers.push_back(marker_);
	}
	//std::cerr << "MARKER ARRAY published with size: " << marker_array_msg_.markers.size() << std::endl; 
	marker_array_pub_.publish(marker_array_msg_);
      }
      cout << "Waiting msg..." << endl;
  }
  
  void loop(){
      cloud_topic_ = "input";
      sub_.subscribe (nh_, "input", 1000,  boost::bind (&ViewAndDetect::vad_cb, this, _1));
      ROS_INFO ("Listening for incoming data on topic %s", nh_.resolveName (cloud_topic_).c_str ());
  }
};

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[]) {
  if( argc != 11 ){
    cerr << "usage: " << argv[0] << " <rank_num> <exist_voxel_num_threshold> [model_pca_filename] <dim_model> <size1> <size2> <size3> <detect_th> <distance_th> /input:=/camera/depth/points2" << endl;
    exit( EXIT_FAILURE );
  }
  ros::init (argc, argv, "detectObj_VOSCH", ros::init_options::AnonymousName);

  voxel_size = Param::readVoxelSize();  //* ボクセルの一辺の長さ（mm）の読み込み
  //voxel.setVoxelSize( voxel_size );

  detect_th = atof( argv[8] );
  distance_th = atof( argv[9] );
  rank_num = atoi( argv[1] );

  //****************************************
  //* 物体検出のための準備
  box_size = Param::readBoxSize_scene();  //* 分割領域の大きさの読み込み

  //* 次元数など
  dim = Param::readDim();  //* 圧縮した特徴ベクトルの次元数の読み込み
  const int dim_model = atoi(argv[4]);  //* 検出対象物体の部分空間の基底の次元数
  if( dim <= dim_model ){
    cerr << "ERR: dim_model should be less than dim(in dim.txt)" << endl; // 注 特徴量の次元数よりも多くなくてはいけません
    exit( EXIT_FAILURE );
  }
  //* 検出ボックスの大きさを決定する
  region_size = box_size * voxel_size;
  float tmp_val = atof(argv[5]) / region_size;
  int size1 = (int)tmp_val;
  if( ( ( tmp_val - size1 ) >= 0.5 ) || ( size1 == 0 ) ) size1++; // 四捨五入
  tmp_val = atof(argv[6]) / region_size;
  int size2 = (int)tmp_val;
  if( ( ( tmp_val - size2 ) >= 0.5 ) || ( size2 == 0 ) ) size2++; // 四捨五入
  tmp_val = atof(argv[7]) / region_size;
  int size3 = (int)tmp_val;
  if( ( ( tmp_val - size3 ) >= 0.5 ) || ( size3 == 0 ) ) size3++; // 四捨五入
  sliding_box_size = size1 * region_size;
  //* 変数をセット
  search_obj.setNormalizeVal( "param/minmax_r.txt" );
  search_obj.setRange( size1, size2, size3 );
  search_obj.setRank( rank_num );
  search_obj.setThreshold( atoi(argv[2]) );
  search_obj.readAxis( argv[3], dim, dim_model, ASCII_MODE_P, false );  //* 検出対象物体の部分空間の基底軸の読み込み

  //* RGB二値化の閾値の読み込み
  Param::readColorThreshold( color_threshold_r, color_threshold_g, color_threshold_b );

  //* 特徴を圧縮する際に使用する主成分軸の読み込み
  PCA_eigen pca;
  pca.read("models/compress_axis", ASCII_MODE_P );
  MatrixXf tmpaxis = pca.Axis();
  MatrixXf axis = tmpaxis.block( 0,0,tmpaxis.rows(),dim );
  MatrixXf axis_t = axis.transpose();
  VectorXf variance = pca.Variance();
  search_obj.setSceneAxis( axis_t, variance, dim );  //* 圧縮部分空間の基底軸のセット

  // 物体検出のための準備 ここまで
  //****************************************

  //*****************************************//
  //* 以下は、saveData.cppのmain関数と同じ *//
  //*****************************************//
  cout << "hoge" << endl;
  // ボクセル（かメッシュ）のプレビューとデータ取得の準備
  ViewAndDetect vad;

  // ループのスタート
  vad.loop();
  ros::spin();

  return 0;
}
