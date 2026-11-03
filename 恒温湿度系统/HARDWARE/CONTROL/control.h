#ifndef __CONTROL_H
#define __CONTROL_H
#include "sys.h"
#include "control_config.h"

/*
 * 恒温恒湿控制逻辑
 *
 * 温湿度分别使用模糊自整定 PID, 输入为放大10倍的卡尔曼估计值。
 * 正输出: 加热/加湿; 负输出: 制冷/除湿。继电器采用慢周期时间比例控制,
 * 带最短启停和换向停机时间, 参数集中在 control_config.h。
 * 冷风风扇在制冷继电器实际开启时按制冷需求调速, 最低启动占空比可配置。
 * 采样失败或超时立即停机, 清空 PID 状态; 故障停机不受最短开启时间限制。
 *
 * 等待刷卡验证期间冷风风扇会暂时停转: PWM 边沿噪声会盖住 RC522 的 13.56MHz
 * 射频前端, 风扇一转起来就刷不上卡; 验证结束后立刻恢复原转速。
 *
 * 报警: 温湿度偏离设定值超过报警阈值时蜂鸣器响, 回到阈值内自动停止。
 *
 * 设定值调整: 按下编码器按键 -> 进入等待刷卡状态 -> 刷到已授权的卡
 *             -> 进入调整状态, 旋转编码器改设定值 -> 再按一次按键退出。
 *             等待刷卡超时会自动放弃。
 */

//----------------- 控制参数 -----------------
#define TEMP_DEFAULT        25      //默认温度设定值 ℃
#define HUMI_DEFAULT        25      //默认湿度设定值 %RH

#define TEMP_MIN            5       //温度设定范围
#define TEMP_MAX            50
#define HUMI_MIN            10      //湿度设定范围
#define HUMI_MAX            90

#define TEMP_ALARM_TH       10       //温度报警阈值, 偏离设定值超过该值报警
#define HUMI_ALARM_TH       10      //湿度报警阈值

#define CARD_WAIT_MS        10000   //等待刷卡超时时间 ms
#define CARD_ID_LEN         4
#define RC522_CHECK_MS      500     //等待刷卡期间检查RC522状态的间隔 ms

//----------------- 系统状态 -----------------
typedef enum
{
    ST_RUN = 0,         //正常运行, 显示并控制
    ST_WAIT_CARD,       //已按下按键, 等待刷卡验证
    ST_ADJ_TEMP,        //验证通过, 正在调温度设定值
    ST_ADJ_HUMI         //验证通过, 正在调湿度设定值
} SysState_t;

typedef struct
{
    SysState_t state;
    u8  temp_set;           //温度设定值
    u8  humi_set;           //湿度设定值
    u8  pend_ch;            //等待刷卡时记录是哪一路编码器触发的
    u32 wait_start;         //等待刷卡的起始时刻(ms)
    u8  card_denied;        //刷到未授权卡的提示标志

    u8  fan;                //各执行机构当前状态, 便于显示: 除湿风扇 1开 0关
    u8  cold_fan;           //冷风风扇 PWM 占空比 0~100(%)
    u8  heater;
    u8  cooler;
    u8  humidifier;
    u8  alarm;
    s16 temp_output_x10;    //PID需求 -1000..1000, 单位0.1%, 不等于继电器状态
    s16 humi_output_x10;
} SysCtrl_t;

extern SysCtrl_t g_sys;

void Control_Init(void);
/* 每个采样周期调用一次, 输入单位0.1℃/0.1%RH, 不是整数℃/%RH。 */
void Control_Update(s16 temp_x10, s16 humi_x10, u8 data_valid);
/* 主循环持续调用, 执行时间比例输出和采样超时保护。 */
void Control_ApplyOutputs(void);
/* 报警判断, 输入同样放大10倍。 */
void Control_Alarm(s16 temp_x10, s16 humi_x10, u8 data_valid);
/* 按键/刷卡/旋钮的状态机, 在主循环中调用 */
void Control_HandleInput(void);
/* 判断卡号是否在授权列表中 */
u8   Control_CardAuthorized(u8 *id);

#endif
