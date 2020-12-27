#ifndef CAN_CONTROLLER_H_
#define CAN_CONTROLLER_H_

#include "mbed.h"
#include "CAN.h"
#include <vector>
#include <motor_status/motor_status.h>
#include <AKROS_bridge_msgs/motor_can_cmd_single.h>
#include "config.h"
#include "../basic_op/basic_op.h"


// CAN Settings
#define CAN_HOST_ID     0
#define CAN_FREQ        1000000

#define CAN_TX_DATA_LENGTH  8
#define CAN_RX_DATA_LENGTH  6


class CAN_controller{
private:
    CAN can;
    bool initializeFlag;
    
    void can_Cb(void);
    void pack_cmd(CANMessage&);
    void unpack_reply(const CANMessage&);
    
public:
    CAN_controller();
    ~CAN_controller(){};
    std::vector<motor_status> motor;

    bool getInitializeFlag(void);
    void startControl(void);

    void add_motor(uint8_t);
    uint8_t find_iterator(uint8_t);
    void can_send(uint8_t);
    uint8_t getMotorNum(void);

    void enter_control_mode(uint8_t id_);
    void exit_control_mode(uint8_t id_);
    void set_position_to_zero(uint8_t id_);

    
};
#endif
