#include <CAN_controller/CAN_controller.h>

// Constructor
CAN_controller::CAN_controller()
  : can(CAN_RX_PIN, CAN_TX_PIN)
{
    can.frequency(CAN_FREQ);
    can.attach(callback(this, &CAN_controller::can_Cb));
    initializeFlag = false;
}



// モータから受け取った情報をmotor_statusに格納
void CAN_controller::can_Cb(void){
    CANMessage msg_;
    if(can.read(msg_)){
        if(msg_.id == CAN_HOST_ID){
            unpack_reply(msg_);
        }
    }
}


// デジタル指令値をCANメッセージに変換（結合といったほうが正しい？）
// 送信先のCAN_IDをこの関数よりも前に入れておけ！
void CAN_controller::pack_cmd(CANMessage &msg_){
    uint8_t id = msg_.id - 1;
    msg_.data[0] = motor[id].position_ref >> 8;
    msg_.data[1] = motor[id].position_ref & 0xFF;
    msg_.data[2] = motor[id].velocity_ref >> 4;
    msg_.data[3] = ((motor[id].velocity_ref & 0xF)<<4) | (motor[id].Kp >> 8);
    msg_.data[4] = motor[id].Kp & 0xFF;
    msg_.data[5] = motor[id].Kd >> 4;
    msg_.data[6] = ((motor[id].Kd & 0xF)<<4) | (motor[id].effort_ref>>8);
    msg_.data[7] = motor[id].effort_ref & 0xFF;
}


// モータから受け取った情報をmotor_statusに格納
// 関節番号とCAN_IDがずれることに留意！
void CAN_controller::can_send(uint8_t id_){
    CANMessage msg_;
    msg_.id = id_+1;
    pack_cmd(msg_);
    can.write(msg_);
}


// enter control mode(モータコントロールモードに入る)
// 配列の添字をCAN_IDとするのではなく，motor_statusに記載されたCAN_IDを用いる！
// （-> 関数の引数は配列の添字．）
// モータを制御するためには必須！
void CAN_controller::enter_control_mode(uint8_t id_){
    CANMessage msg_;
    msg_.id = motor[id_].id;
    msg_.len = CAN_TX_DATA_LENGTH;
    msg_.data[0] = 0xFF;
    msg_.data[1] = 0xFF;
    msg_.data[2] = 0xFF;
    msg_.data[3] = 0xFF;
    msg_.data[4] = 0xFF;
    msg_.data[5] = 0xFF;
    msg_.data[6] = 0xFF;
    msg_.data[7] = 0xFC;
    
    if(can.write(msg_)){
        // pc.printf("Motor %d : Enter control mode \r\n", );
    }
}


// Exit motor control mode
void CAN_controller::exit_control_mode(uint8_t id_){
    CANMessage msg_;
    msg_.id = motor[id_].id;
    msg_.len = CAN_TX_DATA_LENGTH;
    msg_.data[0] = 0xFF;
    msg_.data[1] = 0xFF;
    msg_.data[2] = 0xFF;
    msg_.data[3] = 0xFF;
    msg_.data[4] = 0xFF;
    msg_.data[5] = 0xFF;
    msg_.data[6] = 0xFF;
    msg_.data[7] = 0xFD;
    
    if(can.write(msg_)){
        // pc.printf("Motor %d : Exit control mode \r\n", );
    }
}

// set the current motor position to zero
void CAN_controller::set_position_to_zero(uint8_t id_){
    CANMessage msg_;
    msg_.id = motor[id_].id;
    msg_.len = CAN_TX_DATA_LENGTH;
    msg_.data[0] = 0xFF;
    msg_.data[1] = 0xFF;
    msg_.data[2] = 0xFF;
    msg_.data[3] = 0xFF;
    msg_.data[4] = 0xFF;
    msg_.data[5] = 0xFF;
    msg_.data[6] = 0xFF;
    msg_.data[7] = 0xFE;
    
    if(can.write(msg_)){
        // pc.printf("Set the current motor position to zero \r\n");    // Debug
    }
}