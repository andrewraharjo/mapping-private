#ifndef MY_SEARCH_VOSCH_HPP
#define MY_SEARCH_VOSCH_HPP

#include <pcl/point_types.h>
//#include <pcl/features/feature.h>
#include "pcl/filters/voxel_grid.h"

/***************************************************/
/* 物体検出をするクラス                                */
/* 環境全体からスライディングボックス方式により             */
/* 検出対象物体との類似度が高い領域を上位 rank_num個出力する */
/***************************************************/

//* 検出モード
//* 検出対象物体のrange1、range2、range3のそれぞれをx、y、zのどれに割り当てるか。の6通りで場合分け
enum SearchMode{ S_MODE_1, S_MODE_2, S_MODE_3, S_MODE_4, S_MODE_5, S_MODE_6 };

class SearchObjVOSCH{
public:
  double search_time; // 検出に要する時間
  SearchObjVOSCH();
  ~SearchObjVOSCH();
  void setRange( int _range1, int _range2, int _range3 ); // 検出対象物体のサイズ情報をセット
  void setRank( int _rank_num );   // ランク何位までの領域を出力するか、をセット
  void setThreshold( int _exist_voxel_num_threshold ); // 検出領域の中に含まれるボクセル数がいくつ未満だったら打ちきるかの閾値をセット
  void readAxis( const char *filename, int dim, int dim_model, bool ascii, bool multiple_similarity ); // 検出対象物体の部分空間の基底軸の読み込み
  void readData( const char *filenameF, const char *filenameN, int dim, bool ascii ); // sceneの（分割領域毎の）特徴量とボクセル数の読み込み
  void getRange( int &xrange, int &yrange, int &zrange, SearchMode mode ); // 引数の検出モードにおける検出ボックスのx,y,z辺の長さを取得
  void search(); // 全ての検出モードでの物体検出
  void writeResult( const char *filename, int box_size ); // 結果をファイルに出力
  void cleanMax();

  // オンライン処理むけ
  void setSceneAxis( Eigen3::MatrixXf _axis );
  void setSceneAxis( Eigen3::MatrixXf _axis, Eigen3::VectorXf var, int dim ); // whitening むけ
  void cleanData();
  void setData( int dim, int thR, int thG, int thB, pcl::VoxelGrid<pcl::PointXYZRGBNormal> grid, pcl::PointCloud<pcl::PointXYZRGBNormal> cloud, pcl::PointCloud<pcl::PointXYZRGBNormal> cloud_downsampled, const double voxel_size, const int subdivision_size, const bool is_normalize = false );
  //void setData( pcl::VoxelGrid<>& grid, Voxel& voxel_bin, int dim, int box_size );
  const int XYnum() { return xy_num; }
  const int Znum() { return z_num; }
  const int maxX( int num ){ return max_x[ num ]; }
  const int maxY( int num ){ return max_y[ num ]; }
  const int maxZ( int num ){ return max_z[ num ]; }
  const double maxDot( int num ){ return max_dot[ num ]; }
  const int maxXrange( int num );
  const int maxYrange( int num );
  const int maxZrange( int num );
  void setNormalizeVal( const char* filename );

protected:
  int range1; // 検出対象物体のボックスサイズ（辺1）
  int range2; // 検出対象物体のボックスサイズ（辺2）
  int range3; // 検出対象物体のボックスサイズ（辺3）
  int x_num;  // sceneのx軸上のボックスの数
  int y_num;  // sceneのy軸上のボックスの数
  int z_num;  // sceneのz軸上のボックスの数
  int xy_num; // x_num * _num
  int rank_num; // ランク何位まで出力したいか
  int exist_voxel_num_threshold; // 検出領域の中に含まれるボクセル数がいくつ未満だったら打ちきるかの閾値
  int *max_x; // 検出領域の開始地点のx座標（rank_num位まで保存）
  int *max_y; // 検出領域の開始地点のy座標（rank_num位まで保存）
  int *max_z; // 検出領域の開始地点のz座標（rank_num位まで保存）
  SearchMode *max_mode; // 検出領域を検出したときの検出モード
  double *max_dot; // 検出領域と検出対象物体との類似度（rank_num位まで保存）
  Eigen3::MatrixXf axis_q;   // 検出対象物体の部分空間の基底軸
  int *exist_voxel_num; // 領域内のボクセルの個数
  Eigen3::VectorXf *nFeatures; // 領域の特徴量
  Eigen3::MatrixXf axis_p;  // 圧縮部分空間の基底軸
  std::vector<float> feature_min; // for histogram normalization
  std::vector<float> feature_max; // for histogram normalization

  int xRange( SearchMode mode ); // 引数の検出モードにおける検出ボックスのx辺の長さを返す
  int yRange( SearchMode mode ); // 引数の検出モードにおける検出ボックスのy辺の長さを返す 
  int zRange( SearchMode mode ); // 引数の検出モードにおける検出ボックスのz辺の長さを返す
  int checkOverlap( int x, int y, int z, SearchMode mode ); // 既に検出した領域と被るかどうかをチェック
  void max_cpy( int src_num, int dest_num ); // 既に発見した領域のランクをずらす
  void max_assign( int dest_num, double dot, int x, int y, int z, SearchMode mode ); // 今発見した領域を既に発見した領域と置き換える
  void search_part( SearchMode mode ); // 引数の検出モードでの物体検出
  template <typename T>
  T clipValue( T* ptr, const int x, const int y, const int z, const int xrange, const int yrange, const int zrange ); // 順に値が積分された配列からその位置における値を取り出す
};

class SearchObjVOSCH_multi : public SearchObjVOSCH {
public:
  using SearchObjVOSCH::search_time; // 検出に要する時間
  SearchObjVOSCH_multi();
  ~SearchObjVOSCH_multi();
  void setModelNum( int model_num_ ){ model_num = model_num_; }

  //void setRange( int _range1, int _range2, int _range3 ); // 検出対象物体のサイズ情報をセット
  void setRank( int _rank_num );   // ランク何位までの領域を出力するか、をセット
  //void setThreshold( int _exist_voxel_num_threshold ); // 検出領域の中に含まれるボクセル数がいくつ未満だったら打ちきるかの閾値をセット
  void readAxis( const char **filename, int dim, int dim_model, bool ascii, bool multiple_similarity ); // multi
  //void readData( const char *filenameF, const char *filenameN, int dim, bool ascii ); // sceneの（分割領域毎の）特徴量とボクセル数の読み込み
  //void getRange( int &xrange, int &yrange, int &zrange, SearchMode mode ); // 引数の検出モードにおける検出ボックスのx,y,z辺の長さを取得
  //void search(); // 全ての検出モードでの物体検出
  //* TODO implement the following:
  //void writeResult( const char *filename, int box_size ); // 結果をファイルに出力
  void cleanMax();

  // オンライン処理むけ
  //void setSceneAxis( Eigen3::MatrixXf _axis );
  //void setSceneAxis( Eigen3::MatrixXf _axis, Eigen3::VectorXf var, int dim ); // whitening むけ
  //void cleanData();
  //void setData( int dim, int thR, int thG, int thB, pcl::VoxelGrid<pcl::PointXYZRGBNormal> grid, pcl::PointCloud<pcl::PointXYZRGBNormal> cloud, pcl::PointCloud<pcl::PointXYZRGBNormal> cloud_downsampled, const double voxel_size, const int subdivision_size, const bool is_normalize = false );
  //void setData( pcl::VoxelGrid<>& grid, Voxel& voxel_bin, int dim, int box_size );
  //const int XYnum() { return xy_num; }
  //const int Znum() { return z_num; }
  const int maxX( int m_num, int num ){ return max_x[ m_num ][ num ]; }
  const int maxY( int m_num, int num ){ return max_y[ m_num ][ num ]; }
  const int maxZ( int m_num, int num ){ return max_z[ m_num ][ num ]; }
  const double maxDot( int m_num, int num ){ return max_dot[ m_num ][ num ]; }
  const int maxXrange( int m_num, int num );
  const int maxYrange( int m_num, int num );
  const int maxZrange( int m_num, int num );
  //void setNormalizeVal( const char* filename );

protected:
  int model_num; // number of detection-target objects
  using SearchObjVOSCH::range1; // 検出対象物体のボックスサイズ（辺1）
  using SearchObjVOSCH::range2; // 検出対象物体のボックスサイズ（辺2）
  using SearchObjVOSCH::range3; // 検出対象物体のボックスサイズ（辺3）
  using SearchObjVOSCH::x_num;  // sceneのx軸上のボックスの数
  using SearchObjVOSCH::y_num;  // sceneのy軸上のボックスの数
  using SearchObjVOSCH::z_num;  // sceneのz軸上のボックスの数
  using SearchObjVOSCH::xy_num; // x_num * _num
  using SearchObjVOSCH::rank_num; // ランク何位まで出力したいか
  using SearchObjVOSCH::exist_voxel_num_threshold; // 検出領域の中に含まれるボクセル数がいくつ未満だったら打ちきるかの閾値
  int **max_x; // 検出領域の開始地点のx座標（rank_num位まで保存）
  int **max_y; // 検出領域の開始地点のy座標（rank_num位まで保存）
  int **max_z; // 検出領域の開始地点のz座標（rank_num位まで保存）
  SearchMode **max_mode; // 検出領域を検出したときの検出モード
  double **max_dot; // 検出領域と検出対象物体との類似度（rank_num位まで保存）
  Eigen3::MatrixXf *axis_q;   // 検出対象物体の部分空間の基底軸
  using SearchObjVOSCH::exist_voxel_num; // 領域内のボクセルの個数
  using SearchObjVOSCH::nFeatures; // 領域の特徴量
  using SearchObjVOSCH::axis_p;  // 圧縮部分空間の基底軸
  using SearchObjVOSCH::feature_min; // for histogram normalization
  using SearchObjVOSCH::feature_max; // for histogram normalization
  int checkOverlap( int m_num, int x, int y, int z, SearchMode mode ); // 既に検出した領域と被るかどうかをチェック
  void max_cpy( int m_num, int src_num, int dest_num ); // 既に発見した領域のランクをずらす
  void max_assign( int m_num, int dest_num, double dot, int x, int y, int z, SearchMode mode ); // 今発見した領域を既に発見した領域と置き換える
  void search_part( SearchMode mode ); // 引数の検出モードでの物体検出
};

#endif
