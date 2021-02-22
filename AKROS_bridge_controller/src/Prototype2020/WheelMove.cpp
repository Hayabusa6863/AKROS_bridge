// 単脚用プログラム
// 車輪による移動

#include <iostream>
#include <ros/ros.h>
#include <AKROS_bridge_msgs/motor_cmd.h>
#include <AKROS_bridge_controller/Prototype2020.h>
#include <AKROS_bridge_controller/Interpolator.h>   // choreonoidの補間ライブラリ
#include <AKROS_bridge_msgs/currentState.h>
#include <std_msgs/Float32.h>

#define JOINT_NUM   3

static const double control_frequency = 100.0;  // 制御周期[Hz]

static const double marginTime = 1.0;       // 待機時間[s]
static const double settingTime = 2.0;      // initialPose遷移時間[s]

static const double movingTime = 5.0;       // 移動時間[s]
static const double movingDistance = 1.0;   // 目標移動距離[m]

static const double q_initial_deg[] = {10.0, -20.0};    // 可動角が心配なので，この実験ではこの角度を初期値とした

ros::Publisher cmd_pub;
AKROS_bridge_msgs::motor_cmd cmd;

//ros::Publisher x_pub, z_pub;
//std_msgs::Float32 x_value, z_value;

ros::ServiceClient currentState_client;
AKROS_bridge_msgs::currentState currentState_srv;

int motor_num;
bool initializeFlag = false;
Eigen::VectorXd qref, qref_old;


cnoid::Interpolator<Eigen::VectorXd> joint_trajectory;  // 関節空間での補間器
cnoid::Interpolator<Eigen::Vector2d> leg_trajectory; // 作業空間での補間器
cnoid::Interpolator<Eigen::Vector2d> wheel_trajectory;


int main(int argc, char** argv){
    ros::init(argc, argv, "flexion_controller");
    ros::NodeHandle nh;
    ros::Rate loop_rate(control_frequency);

    cmd_pub = nh.advertise<AKROS_bridge_msgs::motor_cmd>("motor_cmd", 1);
    // x_pub = nh.advertise<std_msgs::Float32>("x", 1);
    // z_pub = nh.advertise<std_msgs::Float32>("z", 1);

    currentState_client = nh.serviceClient<AKROS_bridge_msgs::currentState>("current_state");

    // rosparamからCAN_ID，Kp, Kdを読み込む
    XmlRpc::XmlRpcValue rosparams;
    nh.getParam("motor_list", rosparams);
    motor_num = rosparams.size();

    // モータ個数が違う場合は中止
    if(motor_num != JOINT_NUM){
        ROS_ERROR("the number of motor is not the same as controller");
        ROS_BREAK();
    }

    cmd.motor.resize(motor_num);

    int phase = 0;
    int counter = 0;
    
    qref.resize(JOINT_NUM);
    qref_old.resize(JOINT_NUM);
    qref = Eigen::VectorXd::Zero(JOINT_NUM);
    qref_old = Eigen::VectorXd::Zero(JOINT_NUM);

    // 初期状態を取得
    Eigen::VectorXd q_init(JOINT_NUM);
    q_init = Eigen::VectorXd::Zero(JOINT_NUM);

    Eigen::VectorXd q_target(JOINT_NUM);

    if(currentState_client.call(currentState_srv)){
        for(int i=0; i<JOINT_NUM; i++){
            q_init[i] = currentState_srv.response.reply.motor[i].position;
            // std::cout << "q_init[" << i << "] : " << q_init[i] << std::endl; // debug
        }
        qref_old = q_init;
    }else{
        ROS_ERROR("Failed to get current state !");
    }


    for(auto param_itr=rosparams.begin(); param_itr!=rosparams.end(); ++param_itr){
        cmd.motor[counter].CAN_ID = static_cast<int>(param_itr->second["can_id"]);
        cmd.motor[counter].Kp     = static_cast<double>(param_itr->second["Kp"]);
        cmd.motor[counter].Kd     = static_cast<double>(param_itr->second["Kd"]);
        cmd.motor[counter].effort = 0.0;
        counter++;
    }

    // 各種計算
    Eigen::VectorXd q_initial(2);
    q_initial << deg2rad(q_initial_deg[0]), deg2rad(q_initial_deg[1]);
    Eigen::Vector2d p_init = solve_sagittal_FK(q_initial.head<2>());   // 初期脚先位置
    // x_value.data = p_init[0];
    // z_value.data = p_init[1];

    ROS_INFO("Transition to basic pose ...");
    ros::Time t_start = ros::Time::now();

    while(ros::ok()){
        double current_time = (ros::Time::now() - t_start).toSec() - marginTime; // 現在時刻

        // 待機
        if(phase == 0){
            qref = q_init;
            if(current_time > 0.0){
                initializeFlag = false;
                phase = 1;
            }
        }

        // initialPoseからまずq = [10.0, -20.0][deg]の状態へ
        else if(phase == 1){
            if(!initializeFlag){
                joint_trajectory.clear();
                joint_trajectory.appendSample(current_time, q_init.head<2>());
                joint_trajectory.appendSample(current_time+settingTime, q_initial);
                joint_trajectory.update();
                initializeFlag = true;
            }

            qref.head<2>() = joint_trajectory.interpolate(current_time);

            if(current_time > joint_trajectory.domainUpper()){
                initializeFlag = false;
                phase = 2;
                break;
            }
        }

        for(int i=0; i<JOINT_NUM; i++){
            cmd.motor[i].position = qref[i];
            cmd.motor[i].velocity = (qref[i] / qref_old[i]) / control_frequency;
        }
        qref_old = qref;
        cmd_pub.publish(cmd);
        // x_pub.publish(x_value);
        // z_pub.publish(z_value);

        ros::spinOnce();
        loop_rate.sleep();
    }

    ROS_INFO("send s to start move ...");
    char buff;
    std::cin >> buff;

    ROS_INFO("start moving ...");


    // 車輪移動に関する計算
    t_start = ros::Time::now();

    while(ros::ok()){
        double current_time = (ros::Time::now() - t_start).toSec() - marginTime; // 現在時刻
        
        if(phase == 2){
            if(initializeFlag == false){
                wheel_trajectory.clear();
                wheel_trajectory.appendSample(current_time, Eigen::Vector2d::Zero());
                wheel_trajectory.appendSample(current_time+movingTime, (movingDistance/wheel_D)*Eigen::Vector2d::Ones());
                wheel_trajectory.update();
                initializeFlag = true;
            }

            Eigen::Vector2d q_wheel_buf = wheel_trajectory.interpolate(current_time);
            qref[WHEEL] = q_wheel_buf[0];

            if(current_time > wheel_trajectory.domainUpper()){
                initializeFlag = false;
                break;
            }
        }

        // 車輪は速度制御のみ，他は位置，速度を指令値として与える
        for(int i=0; i<JOINT_NUM; i++){
            cmd.motor[i].position = qref[i];
            cmd.motor[i].velocity = (qref[i] / qref_old[i]) / control_frequency;
        }

        cmd.motor[WHEEL].Kp = 0.0;  // 車輪は速度制御
        qref_old = qref;
        cmd_pub.publish(cmd);

        ros::spinOnce();
        loop_rate.sleep();
    }
    return 0;
}