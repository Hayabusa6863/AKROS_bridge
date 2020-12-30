// モータの初期設定を行うノード
#include <ros/ros.h>
#include <AKROS_bridge_msgs/motor_config.h>
#include <AKROS_bridge_controller/initialize_settings.h>

ros::ServiceClient motor_config_client;
AKROS_bridge_msgs::motor_config motor_config_srv;

int main(int argc, char** argv){
    ros::init(argc, argv, "motor_initializer");
    ros::NodeHandle nh;

    motor_config_client = nh.serviceClient<AKROS_bridge_msgs::motor_config>("motor_config");
    
    // すべてのモータに対して制御モードONに
    motor_config_srv.request.configration_mode = ENTER_CONTROL_MODE;
    motor_config_srv.request.CAN_ID = 1;
    if(motor_config_client.call(motor_config_srv)){
        // 失敗している！
        if(motor_config_srv.response.success){
            ROS_INFO("MOTOR %d initialized", 1);
        }else{
            ROS_ERROR("MOTOR %d Initialization Failed", 1);
        }
    }

    motor_config_srv.request.configration_mode = ENTER_CONTROL_MODE;
    motor_config_srv.request.CAN_ID = 2;
    if(motor_config_client.call(motor_config_srv)){
        // 失敗している！
        if(motor_config_srv.response.success){
            ROS_INFO("MOTOR %d initialized", 2);
        }else{
            ROS_ERROR("MOTOR %d Initialization Failed", 2);
        }
    }

    // initialize_lockをかけてモータ個数を確定
    // 必ずこれを実行すること！
    motor_config_srv.request.configration_mode = INITIALIZE_LOCK;
    motor_config_srv.request.CAN_ID = 0;

    if(motor_config_client.call(motor_config_srv)){
        if(motor_config_srv.response.success){
            ROS_INFO("Motor Initialization Finished !");
        }else{
            ROS_ERROR("Motor Initialization Failed !");
        }
    }
    
    

    return 0;
}