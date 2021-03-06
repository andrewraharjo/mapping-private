#include "color_chlac/grsd_colorCHLAC_tools.h"

using namespace pcl;
#define VERBOSE 1
//#define DIVID_TEST

//-------
//* main
int main( int argc, char** argv ){
  if( argc != 2 ){
    ROS_ERROR ("Need one parameter! Syntax is: %s {input_pointcloud_filename.pcd}\n", argv[0]);
    return(-1);
  }
  //* voxel size (downsample_leaf)
  const double voxel_size = 0.01;

  //* read
  pcl::PointCloud<PointXYZRGB> input_cloud;
  readPoints( argv[1], input_cloud );
  double t1 = my_clock();

  //* compute normals
  pcl::PointCloud<PointXYZRGBNormal> cloud;
  computeNormal( input_cloud, cloud );
  ROS_INFO("Normal compute done in %f seconds.", my_clock()-t1);

  //* voxelize
  pcl::VoxelGrid<PointXYZRGBNormal> grid;
  pcl::PointCloud<PointXYZRGBNormal> cloud_downsampled;
  getVoxelGrid( grid, cloud, cloud_downsampled, voxel_size );

  //* extract - VOSCH -
#ifdef DIVID_TEST
  std::vector< std::vector<float> > vosch;
  extractVOSCH( grid, cloud, cloud_downsampled, vosch, 127, 127, 127, voxel_size, 10 );

#else
  std::vector<float> vosch;
  extractVOSCH( grid, cloud, cloud_downsampled, vosch, 127, 127, 127, voxel_size );
#endif
#ifdef VERBOSE
  //  ROS_INFO("VOSCH %10f", (my_clock()-t1)/input_cloud.points.size());
  ROS_INFO("VOSCH %ld", input_cloud.points.size());
#endif
  //* write
  int length = strlen( argv[1] );
  argv[1][ length-4 ] = '\0';
  char filename[ 300 ];
  sprintf(filename,"%s_VOSCH.pcd",argv[1]);
  writeFeature( filename, vosch );

  return(0);
}
