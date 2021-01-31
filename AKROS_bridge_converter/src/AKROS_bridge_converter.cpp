// 共有資源としてmotor_statusがあり，それをもとに書き込み，読み込みを行う．
// AsyncSpinnerによるマルチスレッドなため，排他処理が必須！

#include <AKROS_bridge_converter/AKROS_bridge_converter.h>

// ここで初期化を全て行うべき！
AKROS_bridge_converter::AKROS_bridge_converter(ros::NodeHandle& nh_)
 : nh(nh_), pnh("~"), spinner(0){
    // Topics
    can_pub   = nh.advertise<AKROS_bridge_msgs::motor_can_cmd>("can_cmd", 1);
    reply_pub = nh.advertise<AKROS_bridge_msgs::motor_reply>("motor_reply", 1);
    cmd_sub   = nh.subscribe("motor_cmd", 1, &AKROS_bridge_converter::motor_cmd_Cb, this);
    can_sub   = nh.subscribe("can_reply", 1, &AKROS_bridge_converter::can_reply_Cb, this);

    // Servers
    enter_CM_server      = nh.advertiseService("enter_control_mode", &AKROS_bridge_converter::enter_CM_Cb, this);
    exit_CM_server       = nh.advertiseService("exit_control_mode", &AKROS_bridge_converter::exit_CM_Cb, this);
    set_PZ_server        = nh.advertiseService("set_position_to_zero", &AKROS_bridge_converter::set_PZ_Cb, this);
    servo_setting_server = nh.advertiseService("servo_setting", &AKROS_bridge_converter::servo_setting_Cb, this);
    motor_lock_server    = nh.advertiseService("motor_lock", &AKROS_bridge_converter::motor_lock_Cb, this);
    current_state_server = nh.advertiseService("current_state", &AKROS_bridge_converter::current_state_Cb, this);
    tweak_control_server = nh.advertiseService("tweak_control", &AKROS_bridge_converter::tweak_control_Cb, this);

    // Clients
    motor_config_client = nh.serviceClient<AKROS_bridge_msgs::motor_config>("motor_config");
    

    // load motor configuration file (.yaml)
    XmlRpc::XmlRpcValue motor_list;
    pnh.getParam("motor_list", motor_list);
    ROS_ASSERT(motor_list.getType() == XmlRpc::XmlRpcValue::TypeArray);
    
    // add motor written in yaml file
    for(int i=0; i<motor_list.size(); i++){
        motor_status m;

        // check whether if necessary information written
        if(!motor_list[i]["name"].valid() || !motor_list[i]["can_id"].valid() || !motor_list[i]["model"].valid()){
            ROS_WARN("No name or can_id or model");
            continue;       
        }
        // get name
        if(motor_list[i]["name"].getType() == XmlRpc::XmlRpcValue::TypeString){
            m.name = static_cast<std::string>(motor_list[i]["name"]);
        }
        // get CAN_ID
        if(motor_list[i]["can_id"].getType() == XmlRpc::XmlRpcValue::TypeInt){
            m.CAN_ID = static_cast<int>(motor_list[i]["can_id"]);
        }
        // get model of the motor
        if(motor_list[i]["model"].getType() == XmlRpc::XmlRpcValue::TypeString){
            m.model = static_cast<std::string>(motor_list[i]["model"]);
        }
        motor.push_back(m);
    }

    motor_num = (uint8_t)motor.size();

    // enter_control_mode
    // motor_lock
    for(int i=0; i<motor_num; i++){
        if(motor[i].model == "AK10-9"){
            //motor[i].P_MAX = 
            //motor[i].P_MIN =
            //motor[i].V_MAX = 
            //motor[i].V_MIN =
        }
        else if(motor[i].model == "AK80-6"){
            //motor[i].P_MAX = 
            //motor[i].P_MIN =
            //motor[i].V_MAX = 
            //motor[i].V_MIN =
        }
        else if(motor[i].model == "AK10-9_OLD"){
            //motor[i].P_MAX = 
            //motor[i].P_MIN =
            //motor[i].V_MAX = 
            //motor[i].V_MIN =
        }
        else if(motor[i].model == "AK80-6_OLD"){
            //motor[i].P_MAX = 
            //motor[i].P_MIN =
            //motor[i].V_MAX = 
            //motor[i].V_MIN =
        }

        // Enter control mode for each motor
        motor_config_srv.request.CAN_ID = motor[i].CAN_ID;
        motor_config_srv.request.configration_mode = ENTER_CONTROL_MODE;

        if(motor_config_client.call(motor_config_srv)){
            if(motor_config_srv.response.success){
                res_.success = true;
                ROS_INFO("Motor %d initialized !", req_.CAN_ID);
            }
        }
    }


    // motor_lock
    motor_config_srv.request.CAN_ID = 0;
    motor_config_srv.request.configration_mode = INITIALIZE_LOCK;
    initializeFlag = true;

    // メモリの動的確保
    can_cmd.motor.resize(motor_num);
    reply.motor.resize(motor_num);

    spinner.start();
}


AKROS_bridge_converter::~AKROS_bridge_converter(void){
    spinner.stop();
}


// motor_statusからcan_cmdに格納
// 一つのモータに対する関数
void AKROS_bridge_converter::pack_cmd(AKROS_bridge_msgs::motor_can_cmd_single &can_cmd_, uint8_t index_){
    std::lock_guard<std::mutex> lock(motor_mutex);
    
    can_cmd_.CAN_ID   = motor[index_].CAN_ID;
    can_cmd_.position = motor[index_].position_ref - motor[index_].error;
    can_cmd_.velocity = motor[index_].velocity_ref;
    can_cmd_.effort   = motor[index_].effort_ref;

    // servo_modeに準じてKp，Kdの値を調整
    if(motor[index_].servo_mode){
        can_cmd_.Kp = motor[index_].Kp;
        can_cmd_.Kd = motor[index_].Kd;
    }else{
        can_cmd_.Kp = 0;
        can_cmd_.Kd = 0;
    }
    //ROS_INFO("pack_cmd");
}


// motor_statusをfloat型に変換してmotor_replyに格納
void AKROS_bridge_converter::pack_reply(AKROS_bridge_msgs::motor_reply_single &reply_, uint8_t index_){
    std::lock_guard<std::mutex> lock(motor_mutex);
    reply_.CAN_ID   = motor[index_].CAN_ID;
    reply_.position = uint_to_float(motor[index_].position + motor[index_].error, motor[index_].P_MIN, motor[index_].P_MAX, POSITION_BIT_NUM);
    reply_.velocity = uint_to_float(motor[index_].velocity, motor[index_].V_MIN, motor[index_].V_MAX, VELOCITY_BIT_NUM);
    reply_.effort   = uint_to_float(motor[index_].effort,   T_MIN, T_MAX, EFFORT_BIT_NUM);

    //ROS_INFO("pack_reply");
}


// motor_cmdをint値に変換してmotor_statusに格納
void AKROS_bridge_converter::unpack_cmd(const AKROS_bridge_msgs::motor_cmd_single& cmd_){
    std::lock_guard<std::mutex> lock(motor_mutex);

    uint8_t index_ = find_index(cmd_.CAN_ID);
    motor[index_].position_ref = float_to_uint(fminf(fmaxf(motor[index_].P_MIN, cmd_.position), motor[index_].P_MAX), motor[index_].P_MIN, motor[index_].P_MAX, POSITION_BIT_NUM);
    motor[index_].velocity_ref = float_to_uint(fminf(fmaxf(motor[index_].V_MIN, cmd_.velocity), motor[index_].V_MAX), motor[index_].V_MIN, motor[index_].V_MAX, VELOCITY_BIT_NUM);
    motor[index_].effort_ref   = float_to_uint(fminf(fmaxf(T_MIN, cmd_.effort), T_MAX), T_MIN, T_MAX, EFFORT_BIT_NUM);
    motor[index_].Kp           = float_to_uint(fminf(fmaxf(KP_MIN, cmd_.Kp), KP_MAX), KP_MIN, KP_MAX, KP_BIT_NUM);
    motor[index_].Kd           = float_to_uint(fminf(fmaxf(KD_MIN, cmd_.Kd), KD_MAX), KD_MIN, KD_MAX, KD_BIT_NUM);
}


// motor_can_replyをそのままmotor_statusに格納
void AKROS_bridge_converter::unpack_can_reply(const AKROS_bridge_msgs::motor_can_reply_single& can_reply_){
    std::lock_guard<std::mutex> lock(motor_mutex);
    uint8_t index_ = find_index(can_reply_.CAN_ID);
    motor[index_].CAN_ID   = can_reply_.CAN_ID;
    motor[index_].position = can_reply_.position;
    motor[index_].velocity = can_reply_.velocity;
    motor[index_].effort   = can_reply_.effort;

    //ROS_INFO("unpack_can_reply");
}


// motor_cmdを受け取ってmotor_statusに保存
void AKROS_bridge_converter::motor_cmd_Cb(const AKROS_bridge_msgs::motor_cmd::ConstPtr& cmd_){
    for(uint8_t i=0; i<cmd_->motor.size(); i++){
        unpack_cmd(cmd_->motor[i]);
    }
    publish_cmd();
}


// /can_replyを受け取って実数値に変換し，motor_statusに格納
void AKROS_bridge_converter::can_reply_Cb(const AKROS_bridge_msgs::motor_can_reply::ConstPtr& can_reply_){
    for(uint8_t i=0; i<can_reply_->motor.size(); i++){
        unpack_can_reply(can_reply_->motor[i]);
    }
    publish_reply();
}


// モータを追加
// Initialize_lockが呼ばれた後は無効
// To deprecated
bool AKROS_bridge_converter::enter_CM_Cb(AKROS_bridge_msgs::enter_control_mode::Request& req_, AKROS_bridge_msgs::enter_control_mode::Response& res_){
    if(!initializeFlag){
        // push_back
        motor_status m;
        m.name = req_.name; // この変換がOKが要確認
        m.CAN_ID = req_.CAN_ID;
        m.model = req_.model;

        motor.push_back(m);

        motor_config_srv.request.CAN_ID = req_.CAN_ID;
        motor_config_srv.request.configration_mode = ENTER_CONTROL_MODE;

        if(motor_config_client.call(motor_config_srv)){
            if(motor_config_srv.response.success){
                res_.success = true;
                ROS_INFO("Motor %d initialized !", req_.CAN_ID);
            }
        }
        return true;    
    }else{
        ROS_ERROR("Motors has been already locked !");
        res_.success = false;
        return false;
    }
}


// 操作終了
// これを行うともう一度原点だしが必要になるので注意
// To deprecated
bool AKROS_bridge_converter::exit_CM_Cb(AKROS_bridge_msgs::exit_control_mode::Request& req_, AKROS_bridge_msgs::exit_control_mode::Response& res_){
    motor_config_srv.request.CAN_ID = req_.CAN_ID;
    motor_config_srv.request.configration_mode = EXIT_CONTROL_MODE;

    // 指定したvector要素のみを削除したい
    // motor.erase(motor.begin() + (unsigned int)find_index[req_.CAN_ID]);    // vectorを削除
    
    if(motor_config_client.call(motor_config_srv)){
        if(motor_config_srv.response.success)
            res_.success = true;
    }
}


// モータの原点と関節の原点との誤差値を設定
bool AKROS_bridge_converter::set_PZ_Cb(AKROS_bridge_msgs::set_position_zero::Request& req_, AKROS_bridge_msgs::set_position_zero::Response& res_){
    motor_config_srv.request.CAN_ID = req_.CAN_ID;
    motor_config_srv.request.configration_mode = SET_POSITION_TO_ZERO;
    
    /* deprecated!
    if(motor_config_client.call(motor_config_srv)){
        if(motor_config_srv.response.success)
            res_.success = true;
    }
    motor[find_index(req_.CAN_ID)].position_ref = 32767;
    motor[find_index(req_.CAN_ID)].velocity_ref = 2047;
    motor[find_index(req_.CAN_ID)].effort_ref = 2047;
    */

    // error = joint - motor
    motor[find_index(req_.CAN_ID)].error = CENTER_POSITION - motor[find_index(req_.CAN_ID)].position;
    motor[find_index(req_.CAN_ID)].position_ref = CENTER_POSITION;

    res_.success = true;

    return true;
}


// motor_cmdのKp，Kdを0にしてモータのサーボをOFFにする
// サーボOFF時に実数目標値を0（整数値ではCENTER_〇〇）に設定
bool AKROS_bridge_converter::servo_setting_Cb(AKROS_bridge_msgs::servo_setting::Request& req_, AKROS_bridge_msgs::servo_setting::Response& res_){
    // CAN_ID = 0ならすべてのモータに対して一括設定
    if(req_.CAN_ID == 0){
        for(uint8_t i=0; i<motor.size(); i++){
            motor[i].servo_mode = req_.servo;
        }

        if(req_.servo){
            ROS_INFO("All servo ON");
        }else{
            ROS_INFO("All servo OFF");
            // 指令値をすべて0に！
            for(size_t i=0; i<motor.size(); i++){
                motor[i].position_ref = CENTER_POSITION;
                motor[i].velocity_ref = CENTER_VELOCITY;
                motor[i].effort_ref = CENTER_EFFORT;
            }
        }
    }else{  // そうでなければ個別に設定
        motor[find_index(req_.CAN_ID)].servo_mode = req_.servo;
        if(req_.servo){
            ROS_INFO("Motor %d servo ON", req_.CAN_ID);
        }else{
            ROS_INFO("Motor %d servo OFF", req_.CAN_ID);
            motor[find_index(req_.CAN_ID)].position_ref = CENTER_POSITION;
            motor[find_index(req_.CAN_ID)].velocity_ref = CENTER_VELOCITY;
            motor[find_index(req_.CAN_ID)].effort_ref = CENTER_EFFORT;
        }
    }
    
    res_.success = true;
    return true;
}


// モータの個数確定
// これ以上のモータ追加は不可能
// To deprecate
bool AKROS_bridge_converter::motor_lock_Cb(std_srvs::Empty::Request& res_, std_srvs::Empty::Response& req_){
    if(!initializeFlag){
        motor_config_srv.request.CAN_ID = 0;
        motor_config_srv.request.configration_mode = INITIALIZE_LOCK;
        initializeFlag = true;

        // メモリの動的確保
        motor_num = motor.size();
        can_cmd.motor.resize(motor_num);
        reply.motor.resize(motor_num);

        ros::param::set("motor_num", motor_num);

        if(motor_config_client.call(motor_config_srv)){
            ROS_INFO("Motors has been locked !");
            ROS_INFO("You have %d Motors", motor_num);
            ROS_INFO("AsyncSpinner Start !");
            return true;
        }
    }else{
        ROS_ERROR("Motors have been already locked !");
        return false;
    }
}


// 微調節
bool AKROS_bridge_converter::tweak_control_Cb(AKROS_bridge_msgs::tweak::Request& req_, AKROS_bridge_msgs::tweak::Response& res_){
switch (req_.control){
case TWEAK_UP:  // +1
    motor[find_index(req_.CAN_ID)].position_ref += tweak_delta;
    res_.success = true;
    return true;
    break;

case TWEAK_DOWN:    // -1
    motor[find_index(req_.CAN_ID)].position_ref -= tweak_delta;
    res_.success = true;
    return true;
    break;

case UP:    // +10
    motor[find_index(req_.CAN_ID)].position_ref += delta;
    res_.success = true;
    return true;
    break;

case DOWN:  // -10
    motor[find_index(req_.CAN_ID)].position_ref -= delta;
    res_.success = true;
    return true;
    break;

case BIG_UP:    // +100
    motor[find_index(req_.CAN_ID)].position_ref += big_delta;
    res_.success = true;
    return true;
    break;

case BIG_DOWN:  // -100
    motor[find_index(req_.CAN_ID)].position_ref -= big_delta;
    res_.success = true;
    return true;
    break;

default:
    return false;
    break;
}
}


// motor_statusの値を返す
bool AKROS_bridge_converter::current_state_Cb(AKROS_bridge_msgs::currentState::Request& req_, AKROS_bridge_msgs::currentState::Response& res_){
    pack_reply(res_.reply, find_index(req_.CAN_ID));
    res_.success = true;
    return true;
}


// CAN指令値をmotor_statusから引っ張り出して変換し，publish 
void AKROS_bridge_converter::publish_cmd(void){
    if(initializeFlag){
        for(uint8_t i=0; i<motor.size(); i++){
            pack_cmd(can_cmd.motor[i], i);
        }
        can_pub.publish(can_cmd);
    }
}


// 応答値をmotor_statusから引っ張り出してpublish
void AKROS_bridge_converter::publish_reply(void){
    if(initializeFlag){
        for(uint8_t i=0; i<motor.size(); i++){
            pack_reply(reply.motor[i], i);
        }
        reply_pub.publish(reply);
    }
}


// モータのCAN_IDからvector<motor_status>のindexを探し出す
uint8_t AKROS_bridge_converter::find_index(uint8_t id_){
    for(uint8_t i=0; i<motor.size(); i++){
        if(motor[i].CAN_ID == id_){
            return i;
        }
    }
    return ERROR_NUM;   // 該当するCAN_IDが無ければぬるぽになるはず
}