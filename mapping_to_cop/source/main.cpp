/***

 Mapping to cop
 klank@in.tum.de
*/


#include <ros/ros.h>
#include <vision_srvs/cop_call.h>
#include <vision_msgs/cop_answer.h>
#include <mapping_srvs/GetPlaneClusters.h>
#include <vision_srvs/srvjlo.h>

using namespace vision_srvs;
using namespace vision_msgs;

#define max(a, b) (((a) > (b))  ?  (a) : (b))

typedef uint64_t uint64;

 void publishToCop(std::string object, ros::NodeHandle &n, std::vector<uint64> cluster_ids);


void printfJloMsg(srvjlo msg)
{  
  int width2 = 4;
  printf("Showing PosId %d with parent %d:\n", (int)msg.response.answer.id, (int)msg.response.answer.parent_id);

  for(int r = 0; r < width2; r++)
  {
    for(int c = 0; c < width2; c++)
    {
        printf( "%f ", msg.response.answer.pose[r * width2 + c]);

    }
    printf("\n");
  }
  width2 = 6;
  for(int r = 0; r < width2; r++)
  {
    for(int c = 0; c < width2; c++)
    {
       printf( "%f ", msg.response.answer.cov[r * width2 + c]);
    }
    printf("\n");
  }
}

/*    "# Message to quiey the lo-service\n"
    "uint64 id		#id of a frame, there should be unique mapping from a tf-name-string to such an id\n"
    "uint64 parent_id        #id of parent frame\n"
    "float64[16] pose	#pose matrix, fully projective 4x4 matrix\n"
    "float64[36] cov         #covariance for 6 dof (xyz, rpy)\n"
    "uint16 type             #fixed connection with the parent (1) or free in space (0 = default)\n"
    */
uint64_t JloRegisterPose(std::vector<double> mat, std::vector<double> uncertainty)
{
  srvjlo msg;
  ros::NodeHandle n;
  ros::ServiceClient client = n.serviceClient<srvjlo>("/located_object", true);

  msg.request.query.id = 0;
  msg.request.query.parent_id = 2;  /*ID of swissranger= 2 !*/
  msg.request.query.type = 0;
  msg.request.query.pose = mat;
  msg.request.query.cov = uncertainty;

  msg.request.command ="update";


  if (!client.call(msg))
  {
    printf("Error in jlo  srv!\n");
    return 1;
  }
    else if (msg.response.error.length() > 0)
  {
    printf("Error from jlo: %s!\n", msg.response.error.c_str());
    return 1;
  }
  printfJloMsg(msg);
  printf("After Transformation:\n");
    
  srvjlo msg_trans;
  msg_trans.request.query.id = msg.response.answer.id;
  msg_trans.request.query.parent_id = 5;  /*ID of swissranger= 2  Camera 4+5!*/
  msg_trans.request.query.type = 0;
       
  msg_trans.request.command ="framequery";
                 
    if (!client.call(msg_trans))
    {
      printf("Error in jlo  srv!\n");
      return 1;
    }
    else if (msg_trans.response.error.length() > 0)
    {
       printf("Error from jlo: %s!\n", msg_trans.response.error.c_str());
       return 1;
    }
    printfJloMsg(msg_trans);
  return msg_trans.response.answer.id;
}

void normalize(double &a,double &b, double &c)
{
  double norm = sqrt(a*a + b*b + c*c);
  a /= norm;
  b /= norm;
  c /= norm;
}

void CrossProduct(double b_x, double b_y,double b_z,double c_x,double c_y,double c_z,double* a_x,double* a_y,double* a_z)
{
    *a_x = b_y*c_z - b_z*c_y;
    *a_y = b_z*c_x - b_x*c_z;
    *a_z = b_x*c_y - b_y*c_x;
}


double scalarproduct(const double &a,const double &b, const double &c, const double &d, const double &e, const double &f)
{
  return a * d + b* e + c*f;
}

/*
float64 a
float64 b
float64 c
float64 d
geometry_msgs/Point32 pcenter
tabletop_msgs/ObjectOnTable[] oclusters
*/
bool GetPlaneClusterCall(std::vector<uint64> &cluster_ids)
{
  mapping_srvs::GetPlaneClusters msg;
  ros::NodeHandle n;
  ros::ServiceClient client = n.serviceClient<mapping_srvs::GetPlaneClusters>("get_plane_clusters_sr", true);
  if (!client.call(msg))
  {
    printf("Error in get_plane_clusters_sr  srv!\n");
    return false;
  }
  double a = msg.response.a;
  double b = msg.response.b;
  double c = msg.response.c;
  double s = msg.response.d;
  geometry_msgs::Point32 &pcenter = msg.response.pcenter;
  std::vector<tabletop_msgs::ObjectOnTable> &vec = msg.response.oclusters;
  printf("Got plane equation %f x + %f y + %f z + %f = 0\n", a,b,c,s);
   /*Norm v1*/
  normalize(a,b,c);

  /*Init v2*/
  double d,e,f,g,h,i;
  if (a == b && a == c)
  {
     d = 1; e = 0; f = 0;
  }
  else
  {
    d = b; e = a; f = c;
  }
  /*Orthogonalize v2*/
  double tmp = scalarproduct(a,b,c,d,e,f);
  d = d - tmp * a;
  e = e - tmp * b;
  f = f - tmp * c;

  /*Norma v2*/
  normalize(d,e,f);

  /*Create v3*/

  CrossProduct(a,b,c,d,e,f, &g,&h,&i);
  /**  Build Matrix:
  *   d g a p.x
  *   e h b p.y
  *   f i c p.z
  *   0 0 0 1
  *   for every cluster
  */
  if(vec.size() == 0)
  {
    printf("No Clusters found, adding a meaningless cluster");
     tabletop_msgs::ObjectOnTable on;
     on.center.x = 0.0;
     on.center.y = -0.1;
     on.center.z = 1.75;
     
     on.min_bound.x = -0.5;
     on.min_bound.y = -0.3;
     on.min_bound.z = 1.6;
     
     on.max_bound.x = 0.5;
     on.max_bound.y = 0.3;
     on.max_bound.z = 1.9;
     vec.push_back(on);
  }
  printf("Creating a pose for every cluster\n");
  for(int i = 0; i < vec.size(); i++)
  {
    printf("a: %f , b: %f , c: %f\n", a, b, c);
    const geometry_msgs::Point32 &center = vec[i].center;
    const geometry_msgs::Point32 &min_bound = vec[i].min_bound;
    const geometry_msgs::Point32 &max_bound = vec[i].max_bound;
    std::vector<double> rotmat;
    rotmat.push_back(d ); rotmat.push_back( g ); rotmat.push_back( a ); rotmat.push_back( center.x);
           rotmat.push_back( e ); rotmat.push_back( h ); rotmat.push_back( b ); rotmat.push_back( center.y);
           rotmat.push_back( f ); rotmat.push_back( i ); rotmat.push_back( c ); rotmat.push_back( center.z);
            rotmat.push_back( 0 ); rotmat.push_back( 0 ); rotmat.push_back( 0 ); rotmat.push_back( 1);
            
    std::vector<double> cov;
    double covx = max(fabs(center.x - max_bound.x), fabs(center.x - min_bound.x)) ;
    double covy = max(fabs(center.y - max_bound.y), fabs(center.y - min_bound.y)) ;
    double covz = max(fabs(center.z - max_bound.z), fabs(center.z - min_bound.z)) ;
    /*Fill covariance with the cluster size and hardcoded full rotation in normal direction */
     cov.push_back(covx); cov.push_back(0    ); cov.push_back( 0    ); cov.push_back( 0   ); cov.push_back( 0   ); cov.push_back( 0);
     cov.push_back( 0    ); cov.push_back( covy ); cov.push_back( 0    ); cov.push_back( 0   ); cov.push_back( 0   ); cov.push_back( 0);
     cov.push_back( 0    ); cov.push_back( 0    ); cov.push_back( covz ); cov.push_back( 0   ); cov.push_back( 0   ); cov.push_back( 0);
     cov.push_back( 0    ); cov.push_back( 0    ); cov.push_back( 0    ); cov.push_back( 0.1 ); cov.push_back( 0   ); cov.push_back( 0);
     cov.push_back( 0    ); cov.push_back( 0    ); cov.push_back( 0    ); cov.push_back( 0   ); cov.push_back( 0.3 ); cov.push_back( 0);
     cov.push_back( 0    ); cov.push_back( 0    ); cov.push_back( 0    ); cov.push_back( 0   ); cov.push_back( 0   ); cov.push_back( 0.3);

     cluster_ids.push_back(JloRegisterPose(rotmat, cov));
     printf("Pushed back a cluster\n");
  }
  return true;
}

std::string _object = "Mug";

 void callback(const boost::shared_ptr<const cop_answer> &msg)
 {
  printf("got answer from cop! (Errors: %s)\n", msg->error.c_str());
  for(int i = 0; i < msg->found_poses.size(); i++)
  {
    const aposteriori_position &pos =  msg->found_poses[i];
    printf("Foub Obj nr %d with prob %f at pos %d\n", (int)pos.objectId, pos.probability, (int)pos.position);
  }
  printf("End!\n");
  printf("Starting enxt round!\n");

  std::vector<uint64> cluster_ids;

  if(GetPlaneClusterCall(cluster_ids))
  {
      ros::NodeHandle n;
      publishToCop(_object, n, cluster_ids);
  }
  else
    exit(0);
 }

 std::string stTopicName = "/tracking/out";
 int _counter = 0;
  
 void publishToCop(std::string object, ros::NodeHandle &n, std::vector<uint64> cluster_ids)
 {
  cop_call call;
  /** Create the cop_call msg*/
  call.request.outputtopic = stTopicName;
  call.request.object_classes.push_back(object);
  call.request.action_type = 0;
  call.request.number_of_objects = 1;
  apriori_position pos;
  int size = (int)cluster_ids.size();
  printf("Number of pos ids? %d\n", size);
  
  for(int i = 0; i < size; i++)
  {
    pos.probability = 1.0 / size;
    pos.positionId = cluster_ids[i];
    call.request.list_of_poses.push_back(pos);
  }
  ros::ServiceClient client = n.serviceClient<cop_call>("/tracking/in", true);
  ros::service::waitForService("/tracking/in", 10000);
  if(!client.call(call))
    printf("Error calling cop");
  else
    printf("Waiting for cops answer on topic\n");
  _counter = 0;
 }

int main(int argc, char* argv[])
{
  ros::init(argc, argv, "/mapping_to_cop") ;
  cop_call call;
  cop_answer answer;
  ros::NodeHandle n;
  if(argc > 1)
    _object = argv[1];
  /** new topic for cop*/

     /** subscribe to the topic cop should publish the results*/
  ros::Subscriber read = n.subscribe<cop_answer>(stTopicName, 1000, &callback);
  /** Publish */
  std::vector<uint64> cluster_ids;

  if(GetPlaneClusterCall(cluster_ids))
  {
      ros::NodeHandle n;
      publishToCop(_object, n, cluster_ids);
  }
  ros::Rate r(0.2);
  /**  Wait for results*/
  while(n.ok())
  {
   _counter++;
    ros::spinOnce();
    r.sleep();
/*    if(_counter > 400)
      publishToCop(_object, n, cluster_ids);*/
  }
  return 0;
}

