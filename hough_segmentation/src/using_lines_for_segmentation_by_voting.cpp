/*
 * Copyright (c) 2011, Lucian Cosmin Goron <goron@in.tum.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// ------------------------------------------------------------------------- //
// -------------------- Specify all needed dependencies -------------------- //
// ------------------------------------------------------------------------- //

#include "ros/ros.h"

#include "terminal_tools/parse.h"

#include "pcl/io/pcd_io.h"
#include "pcl/features/rsd.h"
#include "pcl/features/normal_3d.h"
#include "pcl/filters/extract_indices.h"
#include "pcl/filters/statistical_outlier_removal.h"
#include "pcl/sample_consensus/method_types.h"
#include "pcl/sample_consensus/sac_model_circle.h"
#include "pcl/segmentation/sac_segmentation.h"
#include "pcl/segmentation/extract_clusters.h"

#include "pcl_visualization/pcl_visualizer.h"

#include "pcl_cloud_algos/pcl_cloud_algos_point_types.h"

// --------------------------------------------------------------- //
// -------------------- Declare defs of types -------------------- //
// --------------------------------------------------------------- //

typedef pcl::PointXYZINormal PointT;
typedef pcl::PointXYZINormalRSD PointTrsd;

// --------------------------------------------------------------- //
// -------------------- Define useful structs -------------------- //
// --------------------------------------------------------------- //

struct shape
{
  std::string type;
  pcl::ModelCoefficients::Ptr coefficients;
  pcl::PointIndices::Ptr indices;
  pcl::PointCloud<PointT>::Ptr inliers;
  pcl::PointCloud<PointT>::Ptr cluster;
};

// --------------------------------------------------------------------- //
// -------------------- Declare method's parameters -------------------- //
// --------------------------------------------------------------------- //

int iterations = 100; 

int mean_k_filter = 25; /* [points] */
int std_dev_filter = 1.0; 

// Clustering's Parameters
int minimum_size_of_objects_clusters = 100; /* [points] */
double clustering_tolerance_of_objects = 0.025; /* [meters] */

int line_minimum_size_of_objects_clusters = 10; /* [points] */
double line_clustering_tolerance_of_objects = 0.010; /* [meters] */

// Fitting's Parameters
double   line_threshold = 0.010; /// [meters]
double circle_threshold = 0.010; /// [meters]
double voting_threshold =  0.25; /// [percentage]
double minimum_radius = 0.010; /// [meters]
double maximum_radius = 0.100; /// [meters]
int minimum_line_inliers   = 10; /// [points]
int minimum_circle_inliers = 50; /// [points]
int maximum_line_iterations   = 1000; /// [iterations]
int maximum_circle_iterations = 1000; /// [iterations]
double   line_clustering_tolerance = 0.010; /// [meters]
double circle_clustering_tolerance = 0.010; /// [meters]
int minimum_size_of_line_cluster = 10; /// [points]
int minimum_size_of_circle_cluster = 10; /// [points]

int normals_search_knn = 0; /// [points]
double normals_search_radius = 0.000; /// [meters]
double curvature_threshold = 0.010; /// 
double rsd_search_radius = 0.020; /// [meters]
double rsd_plane_radius = 0.200; /// [meters]
double low_r_min = 0.020; /// [meters]
double high_r_min = 0.060; /// [meters]
double angle_threshold = 45.0; /// [degrees]
double radius_threshold = 0.025; /// [meters]

bool clustering_feature = true;
bool curvature_feature = true;
bool rsd_feature = true;
bool normals_feature = true;
bool percentage_feature = true;

double circle_percentage = 50;
double clustering_tolerance_of_circle_parameters = 0.025;
double minimum_size_of_circle_parameters_clusters = 50;

// Visualization's Parameters
int size = 3;
bool step = false;
bool verbose = false;
bool line_step = false;
bool circle_step = false;
bool circle_feature_step = false;



ofstream textfile;



#define _sqr(c) ((c)*(c))



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Computes line's coefficients with regards to its inliers
 * \param inliers_cloud The point cloud of the line's inliers
 * \param coefficients The line's parameters where the first triplete is a point on the line and the second triplete is the direction
 */
void adjustLine (pcl::PointCloud<PointT>::Ptr &inliers_cloud, pcl::ModelCoefficients &coefficients)
{
  // Vector of line
  double line[2];
  line[0] = coefficients.values [3];
  line[1] = coefficients.values [4];

  // Normalize the vector of line
  double norm = sqrt (line[0]*line[0] + line[1]*line[1]);
  line[0] = line[0] / norm;
  line[1] = line[1] / norm;

  // First point of line
  double P1[2];
  P1[0] = coefficients.values [0];
  P1[1] = coefficients.values [1];

  // Second point of line
  double P2[2];
  P2[0] = coefficients.values [3] + coefficients.values [0];
  P2[1] = coefficients.values [4] + coefficients.values [1];

  // Get limits of inliers leghtwise
  double minimum_lengthwise =  DBL_MAX;
  double maximum_lengthwise = -DBL_MAX;
  for (int idx = 0; idx < (int)inliers_cloud->points.size (); idx++)
  {
    double P[2];
    P[0] = inliers_cloud->points.at(idx).x - P1[0];
    P[1] = inliers_cloud->points.at(idx).y - P1[1];

    double distance_lengthwise = (line[0]*P[0]) + (line[1]*P[1]);
    if (minimum_lengthwise > distance_lengthwise) minimum_lengthwise = distance_lengthwise;
    if (maximum_lengthwise < distance_lengthwise) maximum_lengthwise = distance_lengthwise;
  }

  // New model's coefficients
  P2[0] = P1[0] + line[0]*maximum_lengthwise;
  P2[1] = P1[1] + line[1]*maximum_lengthwise;

  P1[0] = P1[0] + line[0]*minimum_lengthwise;
  P1[1] = P1[1] + line[1]*minimum_lengthwise;

  // Save the new coefficients
  coefficients.values [0] = P1[0];
  coefficients.values [1] = P1[1];
  coefficients.values [2] = 0.0;

  coefficients.values [3] = P2[0] - P1[0];
  coefficients.values [4] = P2[1] - P1[1];
  coefficients.values [5] = 0.0;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** \brief Main routine of the method. Segmentation of point cloud data based on Hough-voting of RANSAC-fitted models.
 */
int main (int argc, char** argv)
{

  // ------------------------------------------------------------------- //
  // -------------------- Check and parse arguments -------------------- //
  // ------------------------------------------------------------------- //

  // Argument check and info about
  if (argc < 2)
  {
    ROS_INFO (" ");
    ROS_INFO ("Syntax is: %s <input>.pcd <options>", argv[0]);
    ROS_INFO ("  where <options> are:");
    ROS_INFO ("    -iterations X                                      = How many times to run the fitting routine.");
    ROS_INFO (" ");
    ROS_INFO ("    -line_threshold X                                  = threshold for line inlier selection");
    ROS_INFO ("    -circle_threshold X                                = threshold for circle inlier selection");
    ROS_INFO ("    -voting_threshold X                                = threshold for Hough-based model voting");
    ROS_INFO ("    -minimum_radius X                                  = ");
    ROS_INFO ("    -maximum_radius X                                  = ");
    ROS_INFO ("    -minimum_line_inliers D                            = ");
    ROS_INFO ("    -minimum_circle_inliers D                          = ");
    ROS_INFO ("    -maximum_line_iterations D                         = ");
    ROS_INFO ("    -maximum_circle_iterations D                       = ");
    ROS_INFO ("    -line_clustering_tolerance X                       = ");
    ROS_INFO ("    -circle_clustering_tolerance X                     = ");
    ROS_INFO (" ");

    ROS_INFO ("    -mean_k_filter X                 = ");
    ROS_INFO ("    -std_dev_filter X                 = ");

    ROS_INFO ("    -minimum_size_of_objects_clusters X                = ");
    ROS_INFO ("    -clustering_tolerance_of_objects X                 = ");
    ROS_INFO (" ");



    ROS_INFO ("    -normals_search_knn X                              = ");
    ROS_INFO ("    -normals_search_radius X                           = ");
    ROS_INFO ("    -curvature_threshold X                             = ");
    ROS_INFO ("    -rsd_search_radius X                               = ");
    ROS_INFO ("    -rsd_plane_radius X                                = ");
    ROS_INFO ("    -low_r_min X                                       = ");
    ROS_INFO ("    -high_r_min X                                      = ");
    ROS_INFO ("    -angle_threshold X                                 = ");
    ROS_INFO ("    -radius_threshold X                                = ");



    ROS_INFO ("    -clustering_feature X                              = ");
    ROS_INFO ("    -curvature_feature X                               = ");
    ROS_INFO ("    -rsd_feature X                                     = ");
    ROS_INFO ("    -normals_feature X                                 = ");
    ROS_INFO ("    -percentage_feature X                              = ");


    ROS_INFO ("    -circle_percentage X                               = ");
    ROS_INFO ("    -clustering_tolerance_of_circle_parameters X       = ");
    ROS_INFO ("    -minimum_size_of_circle_parameters_clusters X      = ");

    ROS_INFO ("    -size B                                            = ");
    ROS_INFO ("    -step B                                            = ");
    ROS_INFO ("    -verbose B                                         = ");
    ROS_INFO ("    -line_step B                                       = wait or not wait");
    ROS_INFO ("    -circle_step B                                     = wait or not wait");
    ROS_INFO ("    -circle_feature_step B                             = wait or not wait");
    ROS_INFO (" ");
    return (-1);
  }

  // Take only the first .pcd file into account
  std::vector<int> pFileIndicesPCD = terminal_tools::parse_file_extension_argument (argc, argv, ".pcd");
  if (pFileIndicesPCD.size () == 0)
  {
    ROS_INFO ("No .pcd file given as input!");
    return (-1);
  }

  terminal_tools::parse_argument (argc, argv,   "-iterations", iterations);

  terminal_tools::parse_argument (argc, argv, "-mean_k_filter", mean_k_filter);
  terminal_tools::parse_argument (argc, argv, "-std_dev_filter", std_dev_filter);

  // Parsing parameters for clustering
  terminal_tools::parse_argument (argc, argv, "-clustering_tolerance_of_objects", clustering_tolerance_of_objects);
  terminal_tools::parse_argument (argc, argv, "-minimum_size_of_objects_clusters", minimum_size_of_objects_clusters);

  terminal_tools::parse_argument (argc, argv, "-line_clustering_tolerance_of_objects", line_clustering_tolerance_of_objects);
  terminal_tools::parse_argument (argc, argv, "-line_minimum_size_of_objects_clusters", line_minimum_size_of_objects_clusters);

  // Parsing the arguments of the method
  terminal_tools::parse_argument (argc, argv,   "-line_threshold",   line_threshold);
  terminal_tools::parse_argument (argc, argv, "-circle_threshold", circle_threshold);
  terminal_tools::parse_argument (argc, argv, "-voting_threshold", voting_threshold);
  terminal_tools::parse_argument (argc, argv, "-minimum_radius", minimum_radius);
  terminal_tools::parse_argument (argc, argv, "-maximum_radius", maximum_radius);
  terminal_tools::parse_argument (argc, argv, "-minimum_line_inliers",   minimum_line_inliers);
  terminal_tools::parse_argument (argc, argv, "-minimum_circle_inliers", minimum_circle_inliers);
  terminal_tools::parse_argument (argc, argv, "-maximum_line_iterations",   maximum_line_iterations);
  terminal_tools::parse_argument (argc, argv, "-maximum_circle_iterations", maximum_circle_iterations);
  terminal_tools::parse_argument (argc, argv,   "-line_clustering_tolerance",   line_clustering_tolerance);
  terminal_tools::parse_argument (argc, argv, "-circle_clustering_tolerance", circle_clustering_tolerance);
  terminal_tools::parse_argument (argc, argv, "-minimum_size_of_line_cluster", minimum_size_of_line_cluster);
  terminal_tools::parse_argument (argc, argv, "-minimum_size_of_circle_cluster", minimum_size_of_circle_cluster);

  terminal_tools::parse_argument (argc, argv, "-normals_search_knn", normals_search_knn);
  terminal_tools::parse_argument (argc, argv, "-normals_search_radius", normals_search_radius);
  terminal_tools::parse_argument (argc, argv, "-curvature_threshold", curvature_threshold);
  terminal_tools::parse_argument (argc, argv, "-rsd_search_radius", rsd_search_radius);  
  terminal_tools::parse_argument (argc, argv, "-rsd_plane_radius", rsd_plane_radius);  
  terminal_tools::parse_argument (argc, argv, "-low_r_min", low_r_min);  
  terminal_tools::parse_argument (argc, argv, "-high_r_min", high_r_min);  
  terminal_tools::parse_argument (argc, argv, "-angle_threshold", angle_threshold);
  terminal_tools::parse_argument (argc, argv, "-radius_threshold", radius_threshold);

  terminal_tools::parse_argument (argc, argv, "-clustering_feature", clustering_feature);  
  terminal_tools::parse_argument (argc, argv, "-curvature_feature", curvature_feature);  
  terminal_tools::parse_argument (argc, argv, "-rsd_feature", rsd_feature);  
  terminal_tools::parse_argument (argc, argv, "-normals_feature", normals_feature);
  terminal_tools::parse_argument (argc, argv, "-percentage_feature", percentage_feature);

  terminal_tools::parse_argument (argc, argv, "-circle_percentage", circle_percentage);
  terminal_tools::parse_argument (argc, argv, "-clustering_tolerance_of_circle_parameters", clustering_tolerance_of_circle_parameters);
  terminal_tools::parse_argument (argc, argv, "-minimum_size_of_circle_parameters_clusters", minimum_size_of_circle_parameters_clusters);

  // Parsing the arguments for visualization
  terminal_tools::parse_argument (argc, argv, "-size", size);
  terminal_tools::parse_argument (argc, argv, "-step", step);
  terminal_tools::parse_argument (argc, argv, "-verbose", verbose);
  terminal_tools::parse_argument (argc, argv, "-line_step", line_step);
  terminal_tools::parse_argument (argc, argv, "-circle_step", circle_step);
  terminal_tools::parse_argument (argc, argv, "-circle_feature_step", circle_feature_step);

  // --------------------------------------------------------- //
  // -------------------- Initializations -------------------- //
  // --------------------------------------------------------- //

  // Initialize random number generator
  srand (time(0));

  // Initialize ros time
  ros::Time::init();

  // Declare the timer
  terminal_tools::TicToc tt;

  // Starting timer
  tt.tic ();

  if ( verbose )
  {
    // Displaying when the timer starts
    ROS_WARN ("Timer started !");
  }

  // --------------------------------------------------- //
  // -------------------- 3D Viewer -------------------- //
  // --------------------------------------------------- //

  // Open a 3D viewer
  pcl_visualization::PCLVisualizer viewer ("3D VIEWER");
  // Set the background of viewer
  viewer.setBackgroundColor (1.0, 1.0, 1.0);
  // Add system coordiante to viewer
  viewer.addCoordinateSystem (1.0f);
  // Parse the camera settings and update the internal camera
  viewer.getCameraParameters (argc, argv);
  // Update camera parameters and render
  viewer.updateCamera ();

  // ----------------------------------------------------------- //
  // ------------------ Load point cloud data ------------------ //
  // ----------------------------------------------------------- //

  // Input point cloud data
  pcl::PointCloud<PointT>::Ptr input_cloud (new pcl::PointCloud<PointT> ());
  // Working point cloud data 
  pcl::PointCloud<PointT>::Ptr working_cloud (new pcl::PointCloud<PointT> ());

  // Load point cloud data
  if (pcl::io::loadPCDFile (argv [pFileIndicesPCD [0]], *input_cloud) == -1)
  {
    ROS_ERROR ("Couldn't read file %s", argv [pFileIndicesPCD [0]]);
    return (-1);
  }

  if ( verbose )
  {
    ROS_INFO ("Loaded %d data points from %s with the following fields: %s", (int) (input_cloud->points.size ()), argv[pFileIndicesPCD[0]], pcl::getFieldsList (*input_cloud).c_str ());
  }

  // Add the point cloud data
  viewer.addPointCloud (*input_cloud, "INPUT");
  // Color the cloud in white
  viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_COLOR, 0.0, 0.0, 0.0, "INPUT");
  // Set the size of points for cloud
  viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, "INPUT"); 

  // Wait or not wait
  if ( step )
  {
    // And wait until Q key is pressed
    viewer.spin ();
  }

  // Update working point cloud
  *working_cloud = *input_cloud;

  // ------------------------------------------------------------------ //
  // ------------------ TEXT FILE W/ SIZES OF MODELS ------------------ //
  // ------------------------------------------------------------------ //

  std::stringstream textname;

//  textname << "cylinder-sizes-" << ros::Time::now() << ".txt";

  textname << "cylinder-sizes-humanoids.txt";

  textfile.open (textname.str ().c_str (), ios::app);

  textfile << "\n" << std::flush;

  std::string p =  argv [pFileIndicesPCD [0]];

  size_t s = p.find_last_of ("/");

  size_t l = p.length ();

  std::string n = p.substr (s + 1, l);

  textfile << "   file " << n << "\n" << std::flush;

  // ------------------------------------------------------------- //
  // ------------------ Filter point cloud data ------------------ //
  // ------------------------------------------------------------- //

  // Filtered point cloud
  pcl::PointCloud<PointT>::Ptr filtered_cloud (new pcl::PointCloud<PointT> ());

  // Create the filtering object
  pcl::StatisticalOutlierRemoval<PointT> sor;
  // Set which point cloud to filter
  sor.setInputCloud (working_cloud);
  // Set number of points for mean distance estimation
  sor.setMeanK (mean_k_filter);
  // Set the standard deviation multiplier threshold
  sor.setStddevMulThresh (std_dev_filter);
  // Call the filtering method
  sor.filter (*filtered_cloud);

  if ( verbose )
  {
    ROS_INFO ("Statistical Outlier Removal ! Before: %d points | After: %d points | Filtered: %d points",
              (int) working_cloud->points.size (),  (int) filtered_cloud->points.size (), (int) working_cloud->points.size () - (int) filtered_cloud->points.size ());
  }

  if ( step )
  {
    // Add the filtered point cloud data in the same viewer
    viewer.addPointCloud (*filtered_cloud, "FILTERED");
    // Color the filtered points in blue
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_COLOR, 0.0, 0.0, 1.0, "FILTERED");
    // Set the size of points for cloud
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, "FILTERED");
    // And wait until Q key is pressed
    viewer.spin ();
  }

  // Update working point cloud
  *working_cloud = *filtered_cloud;

  // ------------------------------------------------------------------- //
  // ------------------ Estimate 3D normals of points ------------------ //
  // ------------------------------------------------------------------- //

  // Point cloud of normals
  pcl::PointCloud<PointT>::Ptr normals_cloud (new pcl::PointCloud<PointT> ());
  // Build kd-tree structure for normals
  pcl::KdTreeFLANN<PointT>::Ptr normals_tree (new pcl::KdTreeFLANN<PointT> ());

  // Create object for normal estimation
  pcl::NormalEstimation<PointT, PointT> ne;
  // Set for which point cloud to compute the normals
  ne.setInputCloud (working_cloud);
  // Provide pointer to the search method
  ne.setSearchMethod (normals_tree);

  if ( normals_search_knn )
  {
    // Set number of k nearest neighbors to use
    ne.setKSearch (normals_search_knn);
  }

  if ( normals_search_radius )
  {
    // Sphere radius used as the maximum distance to consider a point as neighbor
    ne.setRadiusSearch (normals_search_radius);
  }

  // Estimate the normals
  ne.compute (*normals_cloud);

  if ( verbose )
  {
    ROS_INFO ("Normal Estimation ! Returned: %d normals", (int) normals_cloud->points.size ());
  }

  if ( step )
  {
    // Add the point cloud of normals
    viewer.addPointCloudNormals (*working_cloud, *normals_cloud, 1, 0.025, "3D NORMALS");
    // Color the normals with red
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_COLOR, 1.0, 0.0, 0.0, "3D NORMALS"); 
    // And wait until Q key is pressed
    viewer.spin ();

    // Remove the point cloud data
    viewer.removePointCloud ("3D NORMALS");
    // And wait until Q key is pressed
    viewer.spin ();
  }

  // Save the fresh computed normal and curvature values to the working cloud
  for (int idx = 0; idx < (int) working_cloud->points.size (); idx++)
  {
    working_cloud->points.at (idx).normal_x = normals_cloud->points.at (idx).normal_x;
    working_cloud->points.at (idx).normal_y = normals_cloud->points.at (idx).normal_y;
    working_cloud->points.at (idx).normal_z = normals_cloud->points.at (idx).normal_z;
    working_cloud->points.at (idx).curvature = normals_cloud->points.at (idx).curvature;
  }

  if ( step )
  {
    // Set the color handler for curvature
    pcl_visualization::PointCloudColorHandlerGenericField<PointT> curvature_handler (*working_cloud, "curvature");
    // Add the point cloud of curvature values
    viewer.addPointCloud (*working_cloud, curvature_handler, "CURVATURE");
    // Set the size of points for cloud
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, "CURVATURE"); 
    // And wait until Q key is pressed
    viewer.spin ();

    // Remove curvature point cloud
    viewer.removePointCloud ("CURVATURE");
    // And wait until Q key is pressed
    viewer.spin ();
  }

  // Split the planar from the circular curvature points 
  pcl::PointIndices::Ptr curvature_planar_indices (new pcl::PointIndices ());
  pcl::PointIndices::Ptr curvature_circular_indices (new pcl::PointIndices ());

  for (int idx = 0; idx < (int) working_cloud->points.size(); idx++)
  {
    double curvature = working_cloud->points.at (idx).curvature;

    if ( curvature < curvature_threshold )
    {
      // Save the indices from planar surfaces
      curvature_planar_indices->indices.push_back (idx);
    }
    else
    {
      // Save the indices from circular surfaces
      curvature_circular_indices->indices.push_back (idx);
    }
  }

  // Extract the planar and circular curvature points from the working cloud
  pcl::PointCloud<PointT>::Ptr curvature_planar_cloud (new pcl::PointCloud<PointT> ());
  pcl::PointCloud<PointT>::Ptr curvature_circular_cloud (new pcl::PointCloud<PointT> ());

  pcl::ExtractIndices<PointT> curvature_extraction;
  curvature_extraction.setInputCloud (working_cloud);

  curvature_extraction.setIndices (curvature_planar_indices);
  curvature_extraction.setNegative (false);
  curvature_extraction.filter (*curvature_planar_cloud);

  curvature_extraction.setIndices (curvature_circular_indices);
  curvature_extraction.setNegative (false);
  curvature_extraction.filter (*curvature_circular_cloud);

  /*
  if ( step )
  {
    std::stringstream curvature_planar_id;
    curvature_planar_id << "CURVATURE_PLANAR_" << ros::Time::now();
    viewer.addPointCloud (*curvature_planar_cloud, curvature_planar_id.str ());
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, curvature_planar_id.str ()); 
    viewer.spin ();

    viewer.removePointCloud (curvature_planar_id.str());
    viewer.spin ();

    std::stringstream curvature_circular_id;
    curvature_circular_id << "CURVATURE_CIRCULAR_" << ros::Time::now();
    viewer.addPointCloud (*curvature_circular_cloud, curvature_circular_id.str ());
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, curvature_circular_id.str ()); 
    viewer.spin ();

    viewer.removePointCloud (curvature_circular_id.str());
    viewer.spin ();
  }
  */

  if ( step )
  {
    std::stringstream curvature_planar_id;
    curvature_planar_id << "CURVATURE_PLANAR_" << ros::Time::now();
    viewer.addPointCloud (*curvature_planar_cloud, curvature_planar_id.str ());
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, curvature_planar_id.str ()); 

    std::stringstream curvature_circular_id;
    curvature_circular_id << "CURVATURE_CIRCULAR_" << ros::Time::now();
    viewer.addPointCloud (*curvature_circular_cloud, curvature_circular_id.str ());
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, curvature_circular_id.str ()); 

    viewer.spin ();

    viewer.removePointCloud (curvature_planar_id.str());
    viewer.removePointCloud (curvature_circular_id.str());

    viewer.spin ();
  }

  // ----------------------------------------------------------------------- //
  // -------------------- Estimate RSD values of points -------------------- //
  // ----------------------------------------------------------------------- //

  // Create the object for RSD estimation
  pcl::RSDEstimation<PointT, PointT, pcl::PrincipalRadiiRSD> rsd;
  pcl::KdTreeFLANN<PointT>::Ptr rsd_tree (new pcl::KdTreeFLANN<PointT> (working_cloud));

  pcl::PointCloud<pcl::PrincipalRadiiRSD>::Ptr rsd_cloud (new pcl::PointCloud<pcl::PrincipalRadiiRSD> ());
  //pcl::PointCloud<pcl::PrincipalRadiiRSD> rsd_cloud;

  rsd.setInputCloud (working_cloud);
  rsd.setInputNormals (normals_cloud);
  rsd.setRadiusSearch (rsd_search_radius);
  rsd.setPlaneRadius (rsd_plane_radius);
  rsd.setSearchMethod (rsd_tree);
  rsd.compute (*rsd_cloud);

  std::string path =  argv [pFileIndicesPCD [0]];
  size_t fullstop = path.find (".");
  // Create name for saving pcd files
  std::string name = argv [1];
  name.insert (fullstop, "-rsd");

  // Declare the rsd working cloud of points
  pcl::PointCloud<PointTrsd>::Ptr rsd_working_cloud (new pcl::PointCloud<PointTrsd> ());
  // Concatenate radii values with the working cloud
  pcl::concatenateFields (*working_cloud, *rsd_cloud, *rsd_working_cloud);
  // Save these points to disk
//  pcl::io::savePCDFile (name, *rsd_working_cloud);

  if ( step )
  {
    // Set the color handler for curvature
    pcl_visualization::PointCloudColorHandlerGenericField<PointTrsd> rsd_handler (*rsd_working_cloud, "r_min");
    // Add the point cloud of curvature values
    viewer.addPointCloud (*rsd_working_cloud, rsd_handler, "R_MIN");
    // Set the size of points for cloud
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, "R_MIN"); 
    // And wait until Q key is pressed
    viewer.spin ();

    // Remove r min point cloud
    viewer.removePointCloud ("R_MIN");
    // And wait until Q key is pressed
    viewer.spin ();
  }

  // Split the plausible from the implausible r min values
  pcl::PointIndices::Ptr r_min_plausible_indices (new pcl::PointIndices ());
  pcl::PointIndices::Ptr r_min_implausible_indices (new pcl::PointIndices ());

  for (int idx = 0; idx < (int) rsd_working_cloud->points.size(); idx++)
  {
    double r_min = rsd_working_cloud->points.at (idx).r_min;

    if ( (low_r_min < r_min) && (r_min < high_r_min) )
    {
      // Save the right indices of points
      r_min_plausible_indices->indices.push_back (idx);
    }
    else
    {
      // Save the wrong ones
      r_min_implausible_indices->indices.push_back (idx);
    }
  }

  // Extract the plausible and implausible r min values from the working cloud
  pcl::PointCloud<PointT>::Ptr r_min_plausible_cloud (new pcl::PointCloud<PointT> ());
  pcl::PointCloud<PointT>::Ptr r_min_implausible_cloud (new pcl::PointCloud<PointT> ());

  pcl::ExtractIndices<PointT> r_min_extraction;
  r_min_extraction.setInputCloud (working_cloud);

  r_min_extraction.setIndices (r_min_plausible_indices);
  r_min_extraction.setNegative (false);
  r_min_extraction.filter (*r_min_plausible_cloud);

  r_min_extraction.setIndices (r_min_implausible_indices);
  r_min_extraction.setNegative (false);
  r_min_extraction.filter (*r_min_implausible_cloud);

  /*
  if ( step )
  {
    std::stringstream r_min_plausible_id;
    r_min_plausible_id << "CURVATURE_PLANAR_" << ros::Time::now();
    viewer.addPointCloud (*r_min_plausible_cloud, r_min_plausible_id.str ());
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, r_min_plausible_id.str ()); 
    viewer.spin ();

    viewer.removePointCloud (r_min_plausible_id.str());
    viewer.spin ();

    std::stringstream r_min_implausible_id;
    r_min_implausible_id << "CURVATURE_CIRCULAR_" << ros::Time::now();
    viewer.addPointCloud (*r_min_implausible_cloud, r_min_implausible_id.str ());
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, r_min_implausible_id.str ()); 
    viewer.spin ();

    viewer.removePointCloud (r_min_implausible_id.str());
    viewer.spin ();
  }
  */

  if ( step )
  {
    std::stringstream r_min_plausible_id;
    r_min_plausible_id << "R_MIN_PLAUSIBLE_" << ros::Time::now();
    viewer.addPointCloud (*r_min_plausible_cloud, r_min_plausible_id.str ());
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, r_min_plausible_id.str ()); 

    std::stringstream r_min_implausible_id;
    r_min_implausible_id << "R_MIN_IMPLAUSIBLE_" << ros::Time::now();
    viewer.addPointCloud (*r_min_implausible_cloud, r_min_implausible_id.str ());
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, r_min_implausible_id.str ()); 

    viewer.spin ();

    viewer.removePointCloud (r_min_plausible_id.str());
    viewer.removePointCloud (r_min_implausible_id.str());

    viewer.spin ();
  }

  // ATTENTION // 

  // Save the r_min values as intensity values
  for (int idx = 0; idx < (int) working_cloud->points.size (); idx++)
  {
    // This is a quick, but temporary fix of the problem
    working_cloud->points.at (idx).intensity = rsd_cloud->points.at (idx).r_min;
  }

  // ----------------------------------------------------------------------- //
  // -------------------- Estimate 2D normals of points -------------------- //
  // ----------------------------------------------------------------------- //

  for (int idx = 0; idx < (int) normals_cloud->points.size (); idx++)
  {
    // Force normals from 3D in 2D
    normals_cloud->points[idx].normal_z = 0.0;  
  }

  if ( step )
  {
    // Add the normals
    viewer.addPointCloudNormals (*working_cloud, *normals_cloud, 1, 0.025, "2D NORMALS");
    // Color the normals with red
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_COLOR, 0.0, 1.0, 0.0, "2D NORMALS"); 
    // And wait until Q key is pressed
    viewer.spin ();

    // Remove the point cloud data
    viewer.removePointCloud ("2D NORMALS");
    // And wait until Q key is pressed
    viewer.spin ();
  }
 
  for (int idx = 0; idx < (int) normals_cloud->points.size (); idx++)
  {
    double nx = normals_cloud->points[idx].normal_x;
    double ny = normals_cloud->points[idx].normal_y;
    double nz = normals_cloud->points[idx].normal_z;

    double nl = sqrt (nx*nx + ny*ny + nz*nz);
    nx = nx / nl;
    ny = ny / nl;
    nz = nz / nl;

    normals_cloud->points[idx].normal_x = nx;
    normals_cloud->points[idx].normal_y = ny;
    normals_cloud->points[idx].normal_z = nz;
  }

  if ( step )
  {
    // Add the normals
    viewer.addPointCloudNormals (*working_cloud, *normals_cloud, 1, 0.025, "NORMALS");
    // Color the normals with red
    viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_COLOR, 0.0, 0.0, 1.0, "NORMALS"); 
    // And wait until Q key is pressed
    viewer.spin ();

    // Remove the point cloud data
    viewer.removePointCloud ("NORMALS");
    // And wait until Q key is pressed
    viewer.spin ();
  }

  // -------------------------------------------------------------- //
  // ------------------ Cluster point cloud data ------------------ //
  // -------------------------------------------------------------- //

  // Vector of clusters from input cloud
  std::vector<pcl::PointIndices> objects_clusters;
  // Build kd-tree structure for clusters
  pcl::KdTreeFLANN<PointT>::Ptr objects_clusters_tree (new pcl::KdTreeFLANN<PointT> ());

  // Instantiate cluster extraction object
  pcl::EuclideanClusterExtraction<PointT> ece;
  // Set as input the cloud of handle
  ece.setInputCloud (working_cloud);
  // Radius of the connnectivity threshold
  ece.setClusterTolerance (clustering_tolerance_of_objects);
  // Minimum size of clusters
  ece.setMinClusterSize (minimum_size_of_objects_clusters);
  // Provide pointer to the search method
  ece.setSearchMethod (objects_clusters_tree);
  // Call the extraction function
  ece.extract (objects_clusters);

  if ( verbose )
  {
    ROS_INFO ("Euclidean Cluster Extraction ! Returned: %d clusters", (int) objects_clusters.size ());
  }

  // ------------------------------------------------------------- //
  // ------------------ Extract object clusters ------------------ //
  // ------------------------------------------------------------- //

  // Vector of indices which make up the objects clusters
  std::vector<pcl::PointIndices::Ptr> objects_clusters_indices;
  // Point clouds which represent the clusters of the objects
  std::vector<pcl::PointCloud<PointT>::Ptr> objects_clusters_clouds;

  for (int clu = 0; clu < (int) objects_clusters.size(); clu++)
  {
    // Pointer to the cluster object
    pcl::PointIndices::Ptr object_cluster_indices (new pcl::PointIndices (objects_clusters.at (clu)));
    // Cloud of the cluster obejct
    pcl::PointCloud<PointT>::Ptr object_cluster_cloud (new pcl::PointCloud<PointT> ());

    // Extract handle points from the input cloud
    pcl::ExtractIndices<PointT> ei;
    // Set point cloud from where to extract
    ei.setInputCloud (working_cloud);
    // Set which indices to extract
    ei.setIndices (object_cluster_indices);
    // Return the points which represent the inliers
    ei.setNegative (false);
    // Call the extraction function
    ei.filter (*object_cluster_cloud);

    if ( verbose )
    {
      ROS_INFO ("  Object cluster %2d has %4d points", clu, (int) object_cluster_cloud->points.size());
    }

    // Save indices of object
    objects_clusters_indices.push_back (object_cluster_indices);
    // Save the cloud of object cluster
    objects_clusters_clouds.push_back (object_cluster_cloud);
  }

  // --------------------------------------------------------------- //
  // ------------------ Visualize object clusters ------------------ //
  // --------------------------------------------------------------- //

  if ( step )
  {
    // Vector of ids of handles
    std::vector<std::string> objects_clusters_ids;

    for (int clu = 0; clu < (int) objects_clusters_clouds.size(); clu++)
    {
      // Create id for visualization
      std::stringstream object_cluster_id;
      object_cluster_id << "OBJECT_CLUSTER_" << ros::Time::now();

      // Add point cloud to viewer
      viewer.addPointCloud (*objects_clusters_clouds.at (clu), object_cluster_id.str());
      // Set the size of points for cloud
      viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, object_cluster_id.str()); 
      // And wait until Q key is pressed
      viewer.spin ();

      // Save id of object
      objects_clusters_ids.push_back (object_cluster_id.str());
    }
  }

  // -------------------------------------------------------------------------------------------------- //
  // -------------------------------------------------------------------------------------------------- //
  // ------------------------------------ Computation of 2D circles ----------------------------------- //
  // -------------------------------------------------------------------------------------------------- //
  // -------------------------------------------------------------------------------------------------- //

  // Open a 3D viewer
  pcl_visualization::PCLVisualizer circle_viewer ("CIRCLE VIEWER");
  // Set the background of viewer
  circle_viewer.setBackgroundColor (1.0, 1.0, 1.0);
  // Add system coordiante to viewer
  circle_viewer.addCoordinateSystem (1.0f);
  // Parse the camera settings and update the internal camera
  circle_viewer.getCameraParameters (argc, argv);
  // Update camera parameters and render
  circle_viewer.updateCamera ();

  // Add the point cloud data
  circle_viewer.addPointCloud (*working_cloud, "WORKING");
  // Color the cloud in white
  circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_COLOR, 0.0, 0.0, 0.0, "WORKING");
  // Set the size of points for cloud
  circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, "WORKING"); 
  // And wait until Q key is pressed
  circle_viewer.spin ();








  // ------------------------ //
  // Start fitting 2D circles //
  // ------------------------ //

  // Space of parameters for fitted circle models
  pcl::PointCloud<PointT>::Ptr circle_parameters_cloud (new pcl::PointCloud<PointT> ());

  for (int ite = 0; ite < iterations; ite++)
  {
    // Print current iteration number
    ROS_INFO ("AT ITERATION = %d", ite);

    // Vector of circle ids
    std::vector<std::string> circles_ids;
    // Vector of circle inliers ids
    std::vector<std::string> circles_inliers_ids;

    for (int clu = 0; clu < (int) objects_clusters_clouds.size(); clu++)
    {
      int circle_fit = 0;
      bool valid_circle = true;
      bool stop_circle_fitting = false;

      // Working cluster cloud which represents an object
      pcl::PointCloud<PointT>::Ptr working_cluster_cloud (new pcl::PointCloud<PointT> ());
      // Update the working cluster cloud 
      *working_cluster_cloud = *objects_clusters_clouds.at (clu);
        
      do
      {
        // Coefficients of cirlce model
        pcl::ModelCoefficients circle_coefficients;
        // Inliers of circle model
        pcl::PointIndices::Ptr circle_inliers (new pcl::PointIndices ());

        // --------------------- //
        // Start fitting process //
        // --------------------- //

        // Create the segmentation object
        pcl::SACSegmentation<PointT> segmentation_of_circle;
        // Optimize coefficients
        segmentation_of_circle.setOptimizeCoefficients (false);
        // Set type of method
        segmentation_of_circle.setMethodType (pcl::SAC_RANSAC);
        // Set type of model
        segmentation_of_circle.setModelType (pcl::SACMODEL_CIRCLE2D);
        // Set threshold of model
        segmentation_of_circle.setDistanceThreshold (circle_threshold);
        // Set number of maximum iterations
        segmentation_of_circle.setMaxIterations (maximum_circle_iterations);
        // Give as input the filtered point cloud
        segmentation_of_circle.setInputCloud (working_cluster_cloud);
        // Set minimum and maximum radii
        segmentation_of_circle.setRadiusLimits (minimum_radius, maximum_radius);

        // Call the segmenting method
        segmentation_of_circle.segment (*circle_inliers, circle_coefficients);

        // ------------------------ //
        // Start extraction process //
        // ------------------------ //

        // Point cloud of circle inliers
        pcl::PointCloud<PointT>::Ptr circle_inliers_cloud (new pcl::PointCloud<PointT> ());

        // Extract the circular inliers 
        pcl::ExtractIndices<PointT> extraction_of_circle;
        // Set which indices to extract
        extraction_of_circle.setIndices (circle_inliers);
        // Set point cloud from where to extract
        extraction_of_circle.setInputCloud (working_cluster_cloud);

        // Return the points which represent the inliers
        extraction_of_circle.setNegative (false);
        // Call the extraction function
        extraction_of_circle.filter (*circle_inliers_cloud);



        // WARNING //

        // Needed for the bug in pcl extract indices
        int the_actual_number_of_points_from_the_fitted_circle = (int) circle_inliers_cloud->points.size ();



        // START W/ THE CLUSTERING FEATURE //

        if ( clustering_feature )
        {
          if ( (int) circle_inliers->indices.size() < minimum_circle_inliers )
          {
            // The current circle model will be rejected
            valid_circle = false;
          }
          else
          {
            if ( circle_feature_step )
            {
              std::stringstream before_clustering_id;
              before_clustering_id << "CIRCLE_INLIERS_" << ros::Time::now();
              circle_viewer.addPointCloud (*circle_inliers_cloud, before_clustering_id.str ());
              circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, before_clustering_id.str ()); 
              circle_viewer.spin ();
              circle_viewer.removePointCloud (before_clustering_id.str());
              circle_viewer.spin ();
            }

            // Vector of clusters from inliers
            std::vector<pcl::PointIndices> circle_clusters;
            // Build kd-tree structure for clusters
            pcl::KdTreeFLANN<PointT>::Ptr circle_clusters_tree (new pcl::KdTreeFLANN<PointT> ());

            // Instantiate cluster extraction object
            pcl::EuclideanClusterExtraction<PointT> clustering_of_circle;
            // Set as input the cloud of circle inliers
            clustering_of_circle.setInputCloud (circle_inliers_cloud);
            // Radius of the connnectivity threshold
            clustering_of_circle.setClusterTolerance (circle_clustering_tolerance);
            // Minimum number of points of any cluster
            clustering_of_circle.setMinClusterSize (minimum_size_of_circle_cluster);
            // Provide pointer to the search method
            clustering_of_circle.setSearchMethod (circle_clusters_tree);

            // Call the extraction function
            clustering_of_circle.extract (circle_clusters);

            if ( verbose )
            {
              ROS_INFO ("  Model has %d inliers clusters where", (int) circle_clusters.size());
              for (int c = 0; c < (int) circle_clusters.size(); c++)
                ROS_INFO ("    Cluster %d has %d points", c, (int) circle_clusters.at (c).indices.size());
            }

            pcl::PointIndices::Ptr clustering_circle_inliers (new pcl::PointIndices ());

            // Leave only the two biggest clusters
            if ( circle_clusters.size() > 0 )
            {
              for ( int idx = 0; idx < (int) circle_clusters.at (0).indices.size(); idx++ )
              {
                int inl = circle_clusters.at (0).indices.at (idx);
                clustering_circle_inliers->indices.push_back (circle_inliers->indices.at (inl));
              }

              if ( circle_clusters.size() > 1 )
              {
                for ( int idx = 0; idx < (int) circle_clusters.at (1).indices.size(); idx++ )
                {
                  int inl = circle_clusters.at (1).indices.at (idx);
                  clustering_circle_inliers->indices.push_back (circle_inliers->indices.at (inl));
                }
              }
            }

            // ------------------------ //
            // Start extraction process //
            // ------------------------ //

            pcl::PointCloud<PointT>::Ptr clustering_circle_inliers_cloud (new pcl::PointCloud<PointT> ());

            // Extract the circular inliers 
            pcl::ExtractIndices<PointT> clustering_extraction_of_circle;
            // Set which indices to extract
            clustering_extraction_of_circle.setIndices (clustering_circle_inliers);
            // Set point cloud from where to extract
            clustering_extraction_of_circle.setInputCloud (working_cluster_cloud);

            // Return the points which represent the inliers
            clustering_extraction_of_circle.setNegative (false);
            // Call the extraction function
            clustering_extraction_of_circle.filter (*clustering_circle_inliers_cloud);

            if ( circle_feature_step )
            {
              std::stringstream after_clustering_id;
              after_clustering_id << "CIRCLE_INLIERS_" << ros::Time::now();
              circle_viewer.addPointCloud (*clustering_circle_inliers_cloud, after_clustering_id.str ());
              circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, after_clustering_id.str ()); 
              circle_viewer.spin ();
              circle_viewer.removePointCloud (after_clustering_id.str());
              circle_viewer.spin ();
            }

            pcl::PointIndices::Ptr first_cluster_inliers (new pcl::PointIndices ());
            pcl::PointIndices::Ptr second_cluster_inliers (new pcl::PointIndices ());

            if ( circle_clusters.size() > 0 )
            {
              for ( int idx = 0; idx < (int) circle_clusters.at (0).indices.size(); idx++ )
              {
                int inl = circle_clusters.at (0).indices.at (idx);
                first_cluster_inliers->indices.push_back (circle_inliers->indices.at (inl));
              }

              pcl::PointCloud<PointT>::Ptr first_cluster_cloud (new pcl::PointCloud<PointT> ());

              pcl::ExtractIndices<PointT> extraction_of_first_cluster;
              extraction_of_first_cluster.setIndices (first_cluster_inliers);
              extraction_of_first_cluster.setInputCloud (working_cluster_cloud);
              extraction_of_first_cluster.setNegative (false);
              extraction_of_first_cluster.filter (*first_cluster_cloud);

              PointT first_cluster_minimum, first_cluster_maximum;
              pcl::getMinMax3D (*first_cluster_cloud, first_cluster_minimum, first_cluster_maximum);
              double Z1 = first_cluster_maximum.z;

              if ( circle_clusters.size() > 1 )
              {
                for ( int idx = 0; idx < (int) circle_clusters.at (1).indices.size(); idx++ )
                {
                  int inl = circle_clusters.at (1).indices.at (idx);
                  second_cluster_inliers->indices.push_back (circle_inliers->indices.at (inl));
                }

                pcl::PointCloud<PointT>::Ptr second_cluster_cloud (new pcl::PointCloud<PointT> ());

                pcl::ExtractIndices<PointT> extraction_of_second_cluster;
                extraction_of_second_cluster.setIndices (second_cluster_inliers);
                extraction_of_second_cluster.setInputCloud (working_cluster_cloud);
                extraction_of_second_cluster.setNegative (false);
                extraction_of_second_cluster.filter (*second_cluster_cloud);

                PointT second_cluster_minimum, second_cluster_maximum;
                pcl::getMinMax3D (*second_cluster_cloud, second_cluster_minimum, second_cluster_maximum);
                double Z2 = second_cluster_maximum.z;

                if ( fabs (Z1 - Z2) > 0.025 ) /// meters
                {
                  valid_circle = false;
                  circle_inliers->indices.clear ();
                  clustering_circle_inliers->indices.clear ();
                }
              }
            }

            // Update the circle inliers
            *circle_inliers = *clustering_circle_inliers;
            // Update the circle inliers cloud
            *circle_inliers_cloud = *clustering_circle_inliers_cloud;
          }
        }



        // START W/ THE CURVATURE FEATURE //

        if ( curvature_feature )
        {
          if ( (int) circle_inliers->indices.size() < minimum_circle_inliers )
          {
            // The current circle model will be rejected
            valid_circle = false;
          }
          else
          {
            if ( circle_feature_step )
            {
              std::stringstream before_curvature_id;
              before_curvature_id << "CIRCLE_INLIERS_" << ros::Time::now();
              circle_viewer.addPointCloud (*circle_inliers_cloud, before_curvature_id.str ());
              circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, before_curvature_id.str ()); 
              circle_viewer.spin ();
              circle_viewer.removePointCloud (before_curvature_id.str());
              circle_viewer.spin ();
            }

            pcl::PointIndices::Ptr curvature_circle_inliers (new pcl::PointIndices ());

            for (int inl = 0; inl < (int) circle_inliers->indices.size(); inl++)
            {
              int idx = circle_inliers->indices.at (inl);

              double curvature = working_cluster_cloud->points.at (idx).curvature;

              if ( curvature_threshold < curvature )
              {
                // Save the right indices of points
                curvature_circle_inliers->indices.push_back (idx);
              }
            }

            // ------------------------ //
            // Start extraction process //
            // ------------------------ //

            pcl::PointCloud<PointT>::Ptr curvature_circle_inliers_cloud (new pcl::PointCloud<PointT> ());

            // Extract the circular inliers 
            pcl::ExtractIndices<PointT> curvature_extraction_from_circle;
            // Set which indices to extract
            curvature_extraction_from_circle.setIndices (curvature_circle_inliers);
            // Set point cloud from where to extract
            curvature_extraction_from_circle.setInputCloud (working_cluster_cloud);

            // Return the points which represent the inliers
            curvature_extraction_from_circle.setNegative (false);
            // Call the extraction function
            curvature_extraction_from_circle.filter (*curvature_circle_inliers_cloud);

            if ( circle_feature_step )
            {
              std::stringstream after_curvature_id;
              after_curvature_id << "CIRCLE_INLIERS_" << ros::Time::now();
              circle_viewer.addPointCloud (*curvature_circle_inliers_cloud, after_curvature_id.str ());
              circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, after_curvature_id.str ()); 
              circle_viewer.spin ();
              circle_viewer.removePointCloud (after_curvature_id.str());
              circle_viewer.spin ();
            }

            // Update the inliers of circle
            *circle_inliers = *curvature_circle_inliers;
            // Update the points of inliers
            *circle_inliers_cloud = *curvature_circle_inliers_cloud;
          }
        }



        // START W/ THE RSD FEATURE // 

        if ( rsd_feature )
        {
          if ( (int) circle_inliers->indices.size() < minimum_circle_inliers )
          {
            // The current circle model will be rejected
            valid_circle = false;
          }
          else
          {
            if ( circle_feature_step )
            {
              std::stringstream before_rsd_id;
              before_rsd_id << "CIRCLE_INLIERS_" << ros::Time::now();
              circle_viewer.addPointCloud (*circle_inliers_cloud, before_rsd_id.str ());
              circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, before_rsd_id.str ()); 
              circle_viewer.spin ();
              circle_viewer.removePointCloud (before_rsd_id.str());
              circle_viewer.spin ();
            }

            pcl::PointIndices::Ptr rsd_circle_inliers (new pcl::PointIndices ());

            double rc = circle_coefficients.values [2];

            for (int inl = 0; inl < (int) circle_inliers->indices.size(); inl++)
            {
              int idx = circle_inliers->indices.at (inl);

              // ATTENTION // 

              // The intensity values are actually the r_min values 
              //double r_min = working_cluster_cloud->points.at (idx).intensity; 
              double rp = working_cluster_cloud->points.at (idx).intensity;

              //if ( (low_r_min < r_min) && (r_min < high_r_min) ) 
              if ( fabs (rc - rp) < radius_threshold )
              {
                // Save the right indices of points
                rsd_circle_inliers->indices.push_back (idx);
              }
            }

            // ---------------------------- //
            // Start the extraction process //
            // ---------------------------- //

            pcl::PointCloud<PointT>::Ptr rsd_circle_inliers_cloud (new pcl::PointCloud<PointT> ());

            // Extract the circular inliers 
            pcl::ExtractIndices<PointT> rsd_extraction_of_circle;
            // Set which indices to extract
            rsd_extraction_of_circle.setIndices (rsd_circle_inliers);
            // Set point cloud from where to extract
            //rsd_extraction_of_circle.setInputCloud (working_cluster_cloud);
            rsd_extraction_of_circle.setInputCloud (working_cluster_cloud);

            // Return the points which represent the inliers
            rsd_extraction_of_circle.setNegative (false);
            // Call the extraction function
            rsd_extraction_of_circle.filter (*rsd_circle_inliers_cloud);

            if ( circle_feature_step )
            {
              std::stringstream after_rsd_id;
              after_rsd_id << "CIRCLE_INLIERS_" << ros::Time::now();
              circle_viewer.addPointCloud (*rsd_circle_inliers_cloud, after_rsd_id.str ());
              circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, after_rsd_id.str ()); 
              circle_viewer.spin ();
              circle_viewer.removePointCloud (after_rsd_id.str());
              circle_viewer.spin ();
            }

            // Update the inliers of circle
            *circle_inliers = *rsd_circle_inliers;
            // Update the points of inliers
            *circle_inliers_cloud = *rsd_circle_inliers_cloud;
          }
        }



        // START W/ THE NORMALS FEATURE //

        if ( normals_feature )
        {
          if ( (int) circle_inliers->indices.size() < minimum_circle_inliers )
          {
            // The current circle model will be rejected
            valid_circle = false;
          }
          else
          {
            if ( circle_feature_step )
            {
              std::stringstream before_normals_id;
              before_normals_id << "CIRCLE_INLIERS_" << ros::Time::now();
              circle_viewer.addPointCloud (*circle_inliers_cloud, before_normals_id.str ());
              circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, before_normals_id.str ()); 
              circle_viewer.spin ();
              circle_viewer.removePointCloud (before_normals_id.str());
              circle_viewer.spin ();
            }

            pcl::PointIndices::Ptr normals_circle_inliers (new pcl::PointIndices ());

            float c[2];
            c[0] = circle_coefficients.values.at (0);
            c[1] = circle_coefficients.values.at (1);

            for (int inl = 0; inl < (int) circle_inliers->indices.size(); inl++)
            {
              int idx = circle_inliers->indices.at (inl);

              float p[2];
              p[0] = working_cluster_cloud->points.at (idx).x;
              p[1] = working_cluster_cloud->points.at (idx).y;

              float c2p[2];
              c2p[0] = p[0] - c[0];
              c2p[1] = p[1] - c[1];

              float lc2p = sqrt (c2p[0]*c2p[0] + c2p[1]*c2p[1]);
              c2p[0] = c2p[0] / lc2p;
              c2p[1] = c2p[1] / lc2p;

              float np[2];
              np[0] = working_cluster_cloud->points.at (idx).normal_x;
              np[1] = working_cluster_cloud->points.at (idx).normal_y;

              float lnp = sqrt (np[0]*np[0] + np[1]*np[1]);
              np[0] = np[0] / lnp;
              np[1] = np[1] / lnp;

              float dot = c2p[0]*np[0] + c2p[1]*np[1];
              float ang = acos (dot) * 180.0 / M_PI;

              if ( ((180.0 - angle_threshold) < ang) || (ang < angle_threshold) )
              {
                // Save the right indices of points
                normals_circle_inliers->indices.push_back (idx);
              }
            }

            // ---------------------------- //
            // Start the extraction process //
            // ---------------------------- //

            pcl::PointCloud<PointT>::Ptr normals_circle_inliers_cloud (new pcl::PointCloud<PointT> ());

            // Extract the circular inliers 
            pcl::ExtractIndices<PointT> normals_extraction_of_circle;
            // Set which indices to extract
            normals_extraction_of_circle.setIndices (normals_circle_inliers);
            // Set point cloud from where to extract
            normals_extraction_of_circle.setInputCloud (working_cluster_cloud);

            // Return the points which represent the inliers
            normals_extraction_of_circle.setNegative (false);
            // Call the extraction function
            normals_extraction_of_circle.filter (*normals_circle_inliers_cloud);

            if ( circle_feature_step )
            {
              std::stringstream after_normals_id;
              after_normals_id << "CIRCLE_INLIERS_" << ros::Time::now();
              circle_viewer.addPointCloud (*normals_circle_inliers_cloud, after_normals_id.str ());
              circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, after_normals_id.str ()); 
              circle_viewer.spin ();
              circle_viewer.removePointCloud (after_normals_id.str());
              circle_viewer.spin ();
            }

            // Update the inliers of circle
            *circle_inliers = *normals_circle_inliers;
            // Update the points of inliers
            *circle_inliers_cloud = *normals_circle_inliers_cloud;
          }
        }



        // START W/ THE PERCENTAGE FEATURE //
 
        // Percentage of fitted inliers 
        double the_percentage_of_the_remaining_circle_inliers = 0;

        if ( percentage_feature )
        {
          if ( (int) circle_inliers->indices.size() < minimum_circle_inliers )
          {
            // The current circle model will be rejected
            valid_circle = false;
          }
          else
          {
            // Percentage of fitted inleirs 
            the_percentage_of_the_remaining_circle_inliers = round ((double) circle_inliers_cloud->points.size() / (double) the_actual_number_of_points_from_the_fitted_circle * 100);

            if ( the_percentage_of_the_remaining_circle_inliers < circle_percentage )
            {
              // The current circle model will be rejected
              valid_circle = false;
            }
            else
            {
              // The current circle model will be accepted
              valid_circle = true;
            }
          }
        }



        // WARNING //
       
        // Bug in pcl extract indices class, or maybe in pcl filter class 
        if ( (int) working_cluster_cloud->points.size () == the_actual_number_of_points_from_the_fitted_circle )
        {
          // Clear manually the working cluster cloud
          working_cluster_cloud->points.clear ();
        }
        else
        {
          // Return the remaining points of inliers
          extraction_of_circle.setNegative (true);
          // Call the extraction function
          extraction_of_circle.filter (*working_cluster_cloud);
        }



        if ( verbose )
        {
          ROS_INFO ("  The actual number of points from the fitted circle is %d !", the_actual_number_of_points_from_the_fitted_circle);
          ROS_INFO ("  Circle has %d inliers left", (int) circle_inliers_cloud->points.size());
          ROS_INFO ("  %d points remain after extraction", (int) working_cluster_cloud->points.size ());
        }

        if ( !valid_circle )
        {
          ROS_ERROR ("  REJECTED ! %3.0f [%] ! Circle [%2d] has %3d inliers with C = (%6.3f,%6.3f) and R = %5.3f in [%5.3f, %5.3f] found in maximum %d iterations",
                the_percentage_of_the_remaining_circle_inliers, circle_fit, (int) circle_inliers->indices.size (), circle_coefficients.values [0], circle_coefficients.values [1], circle_coefficients.values [2], minimum_radius, maximum_radius, maximum_circle_iterations);

          /*
          // No need for fitting circles anymore
          stop_circle_fitting = true;
          */
        }
        else
        {
          ROS_INFO ("  ACCEPTED ! %3.0f [%] ! Circle [%2d] has %3d inliers with C = (%6.3f,%6.3f) and R = %5.3f in [%5.3f, %5.3f] found in maximum %d iterations",
                the_percentage_of_the_remaining_circle_inliers, circle_fit, (int) circle_inliers->indices.size (), circle_coefficients.values [0], circle_coefficients.values [1], circle_coefficients.values [2], minimum_radius, maximum_radius, maximum_circle_iterations);

          // ------------------------------------- //
          // Build the parameter space for circles //
          // ------------------------------------- //

          // A vote consists of the actual circle parameters
          PointT circle_vot;
          circle_vot.x = circle_coefficients.values [0]; // cx
          circle_vot.y = circle_coefficients.values [1]; // cy
          circle_vot.z = circle_coefficients.values [2]; // r



/*
          double cx = circle_coefficients.values.at (0);
          double cy = circle_coefficients.values.at (1);
          double  r = circle_coefficients.values.at (2);

          for (int idx = 0; idx < (int) circle_inliers_cloud->points.size(); idx++)
          {
            double z = circle_inliers_cloud->points.at (idx).z;

            if ( z < h_of_Z )
            {
              double x = circle_inliers_cloud->points.at (idx).x;
              double y = circle_inliers_cloud->points.at (idx).y;

              double d = sqrt ( _sqr (cx-x) + _sqr (cy-y) ) - r;

              if ( d < circle_threshold ) 
              {
                // Save only the right indices
                inliers->indices.push_back (idx);
              }
            }
          }
*/



/*
          PointT min, max;
          pcl::getMinMax3D (*circle_inliers_cloud, min, max);
          circle_vot.intensity = max.z; // h
*/



          double minimus = +DBL_MAX;
          double maximus = -DBL_MAX;

          for (int point = 0; point < (int) circle_inliers_cloud->points.size(); point++)
          {
            double Z = circle_inliers_cloud->points.at (point).z;

            if ( minimus > Z ) minimus = Z;
            if ( maximus < Z ) maximus = Z;
          }

          circle_vot.intensity = minimus; // h
          circle_vot.curvature = maximus; // h



          // Cast one vot for the current circle
          circle_parameters_cloud->points.push_back (circle_vot);

          // --------------------------- //
          // Start visualization process //
          // --------------------------- //

          if ( circle_step )
          {
            // Create ID for circle model
            std::stringstream circle_id;
            circle_id << "CIRCLE_" << ros::Time::now();

            // Create ID for circle inliers
            std::stringstream circle_inliers_id;
            circle_inliers_id << "CIRCLE_INLIERS_" << ros::Time::now();
 
            // Add circle model to point cloud data
            circle_viewer.addCircle (circle_coefficients, circle_id.str ());

            // Add the point cloud data
            circle_viewer.addPointCloud (*circle_inliers_cloud, circle_inliers_id.str ());

            // Set the size of points for cloud data
            circle_viewer.setPointCloudRenderingProperties (pcl_visualization::PCL_VISUALIZER_POINT_SIZE, size, circle_inliers_id.str ()); 

            // And wait until Q key is pressed
            circle_viewer.spin ();

            // Save circle ids for cleaning the viewer afterwards
            circles_ids.push_back (circle_id.str());
            // Save also circle inliers ids
            circles_inliers_ids.push_back (circle_inliers_id.str());
          }

          // Fit only one model for each cluster in every iteration
          // No need for fitting circles anymore
          //          stop_circle_fitting = true;
        }

        // number of fitted circles
        circle_fit++;


        ///*


        //shape circle;
        //circle.type = "line";
        //circle.coefficients = circle_coefficients;
        //circle.indices = circle_inliers;
        //circle.inliers = circle_inliers_cloud;
        //circle.cluster = working_cluster_cloud;


        //*/



        // --------------------------------------------------- //
        // Check for continuing with the fitting of 2D circles //
        // --------------------------------------------------- //

        // Print the number of points left for model fitting
        if ( (int) working_cluster_cloud->points.size () < minimum_circle_inliers )
          ROS_WARN ("    %d < %d | Stop !", (int) working_cluster_cloud->points.size (), minimum_circle_inliers);
        else
          if ( (int) working_cluster_cloud->points.size () > minimum_circle_inliers )
            ROS_WARN ("    %d > %d | Continue... ", (int) working_cluster_cloud->points.size (), minimum_circle_inliers);
          else
            ROS_WARN ("    %d = %d | Continue... ", (int) working_cluster_cloud->points.size (), minimum_circle_inliers);

      } while ((int) working_cluster_cloud->points.size () > minimum_circle_inliers && stop_circle_fitting == false);
    }

    // ---------------------- //
    // Start cleaning process //
    // ---------------------- //

    if ( circle_step )
    {
      for (int id = 0; id < (int) circles_ids.size(); id++)
      {
        // Remove circle from the viewer
        circle_viewer.removeShape (circles_ids[id]);
      }

      for (int id = 0; id < (int) circles_inliers_ids.size(); id++)
      {
        // Remove circle from the viewer
        circle_viewer.removePointCloud (circles_inliers_ids[id]);
      }

      // And wait until Q key is pressed
      circle_viewer.spin ();
    }
  }

  // -------------------------------------------------------------- //
  // ------------------ Circles Parameters Space ------------------ //
  // -------------------------------------------------------------- //















  exit (0);
}
