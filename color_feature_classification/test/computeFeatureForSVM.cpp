#define QUIET

//* NOTICE
//  Rotation is possible also when feature_type is 'g' or 'r'.

#include <color_feature_classification/points_tools.h>
#include "c3_hlac/c3_hlac_tools.h"
#include "vosch/vosch_tools.h"
#include <terminal_tools/parse.h>
#include <terminal_tools/print.h>

using namespace pcl;
using namespace terminal_tools;

int rotate_step_num = 0;
int thR, thG, thB;
float voxel_size;
int subdivision_size = 0;
int offset_step = 1; // the step size of offset voxel number for subdivisions

void computeFeature( const PointCloud<PointXYZRGB> input_cloud, const char* dir_name, const char feature_type )
{
  int repeat_num_offset = ceil(subdivision_size / offset_step);
  if( subdivision_size == 0 ) repeat_num_offset = 1;
  char filename[ 1000 ];
  int file_num = 0;

  if( feature_type == 'c' ){
    //* voxelize
    pcl::VoxelGrid<PointXYZRGB> grid;
    pcl::PointCloud<PointXYZRGB> cloud_downsampled;
    getVoxelGrid( grid, input_cloud, cloud_downsampled, voxel_size );
    
    // repeat with changing offset values for subdivisions
    for( int ox = 0; ox < repeat_num_offset; ox++ ){
      for( int oy = 0; oy < repeat_num_offset; oy++ ){
	for( int oz = 0; oz < repeat_num_offset; oz++ ){
	  std::vector< std::vector<float> > colorCHLAC;
	  extractC3HLACSignature981( grid, cloud_downsampled, colorCHLAC, thR, thG, thB, voxel_size, subdivision_size, ox*offset_step, oy*offset_step, oz*offset_step );
	  const int hist_num = colorCHLAC.size(); // number of subdivisions
	  
	  for( int h=0; h<hist_num; h++ ){
	    sprintf( filename, "%s/%03d.pcd", dir_name, file_num++ );
	    writeFeature( filename, colorCHLAC[ h ] );
	  }
	}
      }
    }
  }
  else{ // feature_type == 'g' or 'r' or 'd'

    //* compute normals
    pcl::PointCloud<PointXYZRGBNormal> cloud;
    computeNormal( input_cloud, cloud );
    
    //* voxelize
    pcl::VoxelGrid<PointXYZRGBNormal> grid;
    pcl::PointCloud<PointXYZRGBNormal> cloud_downsampled;
    getVoxelGrid( grid, cloud, cloud_downsampled, voxel_size );
    
    // repeat with changing offset values for subdivisions
    for( int ox = 0; ox < repeat_num_offset; ox++ ){
      for( int oy = 0; oy < repeat_num_offset; oy++ ){
	for( int oz = 0; oz < repeat_num_offset; oz++ ){
	  //* extract - GRSD -
	  std::vector< std::vector<float> > grsd;

	  // if( feature_type == 'r' )
	  //   extractPlusGRSDSignature110( grid, cloud, cloud_downsampled, grsd, voxel_size, subdivision_size, ox*offset_step, oy*offset_step, oz*offset_step );
	  // else
	    extractGRSDSignature21( grid, cloud, cloud_downsampled, grsd, voxel_size, subdivision_size, ox*offset_step, oy*offset_step, oz*offset_step );

	  const int hist_num = grsd.size(); // number of subdivisions
		
	  if( feature_type == 'g' ){
	    for( int h=0; h<hist_num; h++ ){
	      sprintf( filename, "%s/%03d.pcd", dir_name, file_num++ );
	      writeFeature( filename, grsd[ h ] );
	    }
	  }
	  else{ // feature_type == 'r' or 'd'
	    //* extract - ColorCHLAC
	    std::vector< std::vector<float> > colorCHLAC;
	    if( feature_type == 'r' )
	      extractC3HLACSignature117( grid, cloud_downsampled, colorCHLAC, thR, thG, thB, voxel_size, subdivision_size, ox*offset_step, oy*offset_step, oz*offset_step );
	    else
	      extractC3HLACSignature981( grid, cloud_downsampled, colorCHLAC, thR, thG, thB, voxel_size, subdivision_size, ox*offset_step, oy*offset_step, oz*offset_step );

	    for( int h=0; h<hist_num; h++ ){
	      sprintf( filename, "%s/%03d.pcd", dir_name, file_num++ );
	      writeFeature( filename, concVector( grsd[ h ], colorCHLAC[ h ] ) );
	    }
	  }
	}
      }
    }
  }
}

void computeFeature_with_rotate( const PointCloud<PointXYZRGB> input_cloud, const char* dir_name, const char feature_type )
{
  int repeat_num_offset = ceil(subdivision_size / offset_step);
  if( subdivision_size == 0 ) repeat_num_offset = 1;
  char filename[ 1000 ];
  int file_num = 0;

  std::vector< std::vector<float> > grsd;
  std::vector< std::vector<float> > colorCHLAC;
    
  if( feature_type == 'c' ){
    pcl::PointCloud<PointXYZRGB> input_cloud_r; // rotate
    pcl::VoxelGrid<PointXYZRGB> grid;
    pcl::PointCloud<PointXYZRGB> cloud_downsampled;

    for(int r3=0; r3 < rotate_step_num; r3++){
      for(int r2=0; r2 < rotate_step_num; r2++){
	for(int r1=0; r1 < rotate_step_num; r1++){
	  const double roll  = r3 * M_PI / (2*rotate_step_num);
	  const double pan   = r2 * M_PI / (2*rotate_step_num);
	  const double roll2 = r1 * M_PI / (2*rotate_step_num);
	
	  rotatePoints( input_cloud, input_cloud_r, roll, pan, roll2 );
	  getVoxelGrid( grid, input_cloud_r, cloud_downsampled, voxel_size );
	    	
	  // repeat with changing offset values for subdivisions
	  for( int ox = 0; ox < repeat_num_offset; ox++ ){
	    for( int oy = 0; oy < repeat_num_offset; oy++ ){
	      for( int oz = 0; oz < repeat_num_offset; oz++ ){
		//* extract features
		extractC3HLACSignature981( grid, cloud_downsampled, colorCHLAC, thR, thG, thB, voxel_size, subdivision_size, ox*offset_step, oy*offset_step, oz*offset_step );	      
		const int hist_num = colorCHLAC.size();
	      
		for( int h=0; h<hist_num; h++ ){
		  sprintf( filename, "%s/%03d.pcd", dir_name, file_num++ );
		  writeFeature( filename, colorCHLAC[ h ] );
		
		  // std::vector<float> colorCHLAC_rotate;
		  // std::vector<float> colorCHLAC_rotate_pre = colorCHLAC[ h ];
		  // std::vector<float> colorCHLAC_rotate_pre2;
		
		  // for(int t=0;t<3;t++){
		  //   rotateFeature90( colorCHLAC_rotate,colorCHLAC_rotate_pre,R_MODE_2);
		  //   feature.push_back (colorCHLAC_rotate);
		  //   colorCHLAC_rotate_pre = colorCHLAC_rotate;
		  // }
		
		  // rotateFeature90( colorCHLAC_rotate,colorCHLAC[ h ],R_MODE_3);
		  // feature.push_back (colorCHLAC_rotate);
		  // colorCHLAC_rotate_pre  = colorCHLAC_rotate;
		  // colorCHLAC_rotate_pre2 = colorCHLAC_rotate;
		
		  // for(int t=0;t<3;t++){
		  //   rotateFeature90( colorCHLAC_rotate,colorCHLAC_rotate_pre,R_MODE_2);
		  //   feature.push_back (colorCHLAC_rotate);
		  //   colorCHLAC_rotate_pre = colorCHLAC_rotate;
		  // }
		
		  // rotateFeature90( colorCHLAC_rotate,colorCHLAC_rotate_pre2,R_MODE_3);
		  // feature.push_back (colorCHLAC_rotate);
		  // colorCHLAC_rotate_pre = colorCHLAC_rotate;
		  // colorCHLAC_rotate_pre2 = colorCHLAC_rotate;
		
		  // for(int t=0;t<3;t++){
		  //   rotateFeature90( colorCHLAC_rotate,colorCHLAC_rotate_pre,R_MODE_2);
		  //   feature.push_back (colorCHLAC_rotate);
		  //   colorCHLAC_rotate_pre = colorCHLAC_rotate;
		  // }
		
		  // rotateFeature90( colorCHLAC_rotate,colorCHLAC_rotate_pre2,R_MODE_3);
		  // feature.push_back (colorCHLAC_rotate);
		  // colorCHLAC_rotate_pre = colorCHLAC_rotate;
		  // //colorCHLAC_rotate_pre2 = colorCHLAC_rotate;
		
		  // for(int t=0;t<3;t++){
		  //   rotateFeature90( colorCHLAC_rotate,colorCHLAC_rotate_pre,R_MODE_2);
		  //   feature.push_back (colorCHLAC_rotate);
		  //   colorCHLAC_rotate_pre = colorCHLAC_rotate;
		  // }
		
		  // rotateFeature90( colorCHLAC_rotate,colorCHLAC[ h ],R_MODE_1);
		  // feature.push_back (colorCHLAC_rotate);
		  // colorCHLAC_rotate_pre = colorCHLAC_rotate;
		
		  // for(int t=0;t<3;t++){
		  //   rotateFeature90( colorCHLAC_rotate,colorCHLAC_rotate_pre,R_MODE_2);
		  //   feature.push_back (colorCHLAC_rotate);
		  //   colorCHLAC_rotate_pre = colorCHLAC_rotate;
		  // }
		
		  // rotateFeature90( colorCHLAC_rotate,colorCHLAC[ h ],R_MODE_4);
		  // feature.push_back (colorCHLAC_rotate);
		  // colorCHLAC_rotate_pre = colorCHLAC_rotate;
		
		  // for(int t=0;t<3;t++){
		  //   rotateFeature90( colorCHLAC_rotate,colorCHLAC_rotate_pre,R_MODE_2);
		  //   feature.push_back (colorCHLAC_rotate);
		  //   colorCHLAC_rotate_pre = colorCHLAC_rotate;
		  // }
		}
	      }
	    }
	  }
	
	  if(r2==0) break;
	}
      }
    }
  }
  else{  // feature_type == 'g' or 'r' or 'd'
    pcl::PointCloud<PointXYZRGBNormal> cloud_normal;
    pcl::PointCloud<PointXYZRGBNormal> cloud_normal_r;
    pcl::VoxelGrid<PointXYZRGBNormal> grid_normal;
    pcl::PointCloud<PointXYZRGBNormal> cloud_downsampled_normal;
	
    //* compute normals
    computeNormal( input_cloud, cloud_normal );
    
    for(int r3=0; r3 < rotate_step_num; r3++){
      for(int r2=0; r2 < rotate_step_num; r2++){
	for(int r1=0; r1 < rotate_step_num; r1++){
	  const double roll  = r3 * M_PI / (2*rotate_step_num);
	  const double pan   = r2 * M_PI / (2*rotate_step_num);
	  const double roll2 = r1 * M_PI / (2*rotate_step_num);
	  
	  //* voxelize
	  rotatePoints( cloud_normal, cloud_normal_r, roll, pan, roll2 );
	  getVoxelGrid( grid_normal, cloud_normal_r, cloud_downsampled_normal, voxel_size );	      
	  
	  // repeat with changing offset values for subdivisions
	  for( int ox = 0; ox < repeat_num_offset; ox++ ){
	    for( int oy = 0; oy < repeat_num_offset; oy++ ){
	      for( int oz = 0; oz < repeat_num_offset; oz++ ){
		//* extract - GRSD -
		// if( feature_type == 'r' )
		//   extractPlusGRSDSignature110( grid_normal, cloud_normal_r, cloud_downsampled_normal, grsd, voxel_size, subdivision_size, ox*offset_step, oy*offset_step, oz*offset_step );
		// else
		  extractGRSDSignature21( grid_normal, cloud_normal_r, cloud_downsampled_normal, grsd, voxel_size, subdivision_size, ox*offset_step, oy*offset_step, oz*offset_step );
		const int hist_num = grsd.size();

		if( feature_type == 'g' ){
		  for( int h=0; h<hist_num; h++ ){
		    sprintf( filename, "%s/%03d.pcd", dir_name, file_num++ );
		    writeFeature( filename, grsd[ h ] );
		  }
		}
		else{ // feature_type == 'r' or 'd'
		  //* extract - ColorCHLAC
		  if( feature_type == 'r' )
		    extractC3HLACSignature117( grid_normal, cloud_downsampled_normal, colorCHLAC, thR, thG, thB, voxel_size, subdivision_size, ox*offset_step, oy*offset_step, oz*offset_step );
		  else
		    extractC3HLACSignature981( grid_normal, cloud_downsampled_normal, colorCHLAC, thR, thG, thB, voxel_size, subdivision_size, ox*offset_step, oy*offset_step, oz*offset_step );
		  
		  for( int h=0; h<hist_num; h++ ){
		    sprintf( filename, "%s/%03d.pcd", dir_name, file_num++ );
		    writeFeature( filename, concVector( grsd[ h ], colorCHLAC[ h ] ) );
		  }
		}
	      }
	    }
	  }
	  
	  if(r2==0) break;
	}
      }
    }
  }
}

//-------
//* main
int main( int argc, char** argv ){
  if( argc < 4 ){
    ROS_ERROR ("Need at least three parameters! Syntax is: %s {input_pointcloud_filename.pcd} {feature_initial(g, c, d, or r)} [options] {config_txt_path} {output_histogram_filename.pcd}\n", argv[0]);
    ROS_INFO ("                          -rotate rotate_step_num = e.g. 3 for 30 degrees rotation\n");
    ROS_INFO ("                          -subdiv N = subdivision size (e.g. 10 voxels)\n");
    ROS_INFO ("                          -offset n = offset step for subdivisions (e.g. 5 voxels)\n");
    return(-1);
  }

  // check feature_type
  const char feature_type = argv[2][0];
  if( (feature_type != 'c') && (feature_type != 'g') && (feature_type != 'd') && (feature_type != 'r') ){
    ROS_ERROR ("Unknown feature type.\n");
    return(-1);
  }

  // rotate step num
  if( parse_argument (argc, argv, "-rotate", rotate_step_num) > 0 ){
    if ( rotate_step_num < 1 ){
      print_error ("Invalid rotate_step_num (%d)!\n", rotate_step_num);
      return (-1);
    }
  }

  // color threshold
  char color_threshold_file[1024];
  sprintf(color_threshold_file, "%s/color_threshold.txt", argv[argc-2]);
  FILE *fp = fopen( color_threshold_file, "r" );
  fscanf( fp, "%d %d %d\n", &thR, &thG, &thB );
  fclose(fp);

  //* voxel size (downsample_leaf)
  char voxel_size_file[1024];
  sprintf(voxel_size_file, "%s/voxel_size.txt", argv[argc-2]);
  fp = fopen( voxel_size_file, "r" );
  fscanf( fp, "%f\n", &voxel_size );
  fclose(fp);

  // subdivision size
  if( parse_argument (argc, argv, "-subdiv", subdivision_size) > 0 ){
    if ( subdivision_size < 0 ){
      print_error ("Invalid subdivision size (%d)! \n", subdivision_size);
      return (-1);
    }
  }

  // offset step
  if( parse_argument (argc, argv, "-offset", offset_step) > 0 ){
    if ( ( offset_step < 1 ) || ( offset_step >= subdivision_size ) ){
      print_error ("Invalid offset step (%d)! (while subdivision size is %d.)\n", offset_step, subdivision_size);
      return (-1);
    }
  }

  //* read
  pcl::PointCloud<PointXYZRGB> input_cloud;
  readPoints( argv[1], input_cloud );

  //* compute feature
  //std::vector< std::vector<float> > feature;
  if( rotate_step_num == 0 )
    computeFeature( input_cloud, argv[ argc - 1 ], feature_type );
  else
    computeFeature_with_rotate( input_cloud, argv[ argc - 1 ], feature_type );
  
  // //* write
  // writeFeature( argv[ argc - 1 ], feature );
  
  return(0);
}
