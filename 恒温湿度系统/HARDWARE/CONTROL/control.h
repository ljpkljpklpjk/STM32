#ifndef __CONTROL_H
#define __CONTROL_H
#include "sys.h"

/*
 * 恒温恒湿控制逻辑
 *
 * 控制策略: 带回差(迟滞)的开关控制。
 *   温度: 低于 设定值-回差 开加热; 高于 设定值+回差 开制冷;
 *         回到设定值附近(进入回差带内)则停止, 避免继电器频繁动作。
 *   湿度: 低于 设定值-回差 开加湿器; 高于 设定值+回差 开除湿风扇;
 *         回到回差带内则关闭。
 *
 * 冷风风扇(装在制冷片旁边, 把冷气吹向四周的那个)不用开关控制, 而是用 PWM
 * 单独调速, 转速只跟"室温比目标温度高多少"有关, 与其它执行机构无关:
 *   室温 <= 目标温度            -> 停转(占空比 0%)
 *   目标温度 < 室温 < 目标温度+10℃ -> 线性增速, 每高1℃加10%占空比
 *   室温 >= 目标温度+10℃        -> 满速(占空比 100%)
 * 若传感器数据无效, 冷风风扇同样停转。
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

#define TEMP_HYST           2       //温度控制回差 ℃
#define HUMI_HYST           3       //湿度控制回差 %RH

/*
 * 冷风风扇调速参数: 占空比 = (室温-目标温度) * COLDFAN_DUTY_PER_DT,
 * 到达 COLDFAN_FULL_DT 度及以上封顶为满速, 不高于目标温度则为 0(停转)。
 */
#define COLDFAN_FULL_DT     5      //满速对应的温差 ℃
#define COLDFAN_DUTY_PER_DT 20      //温差每高1℃增加的占空比 %

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
} SysCtrl_t;

extern SysCtrl_t g_sys;

void Control_Init(void);
/* 温湿度闭环控制, 传入滤波后的实测值 */
void Control_Update(u8 temp, u8 humi, u8 data_valid);
/* 报警判断 */
void Control_Alarm(u8 temp, u8 humi, u8 data_valid);
/* 按键/刷卡/旋钮的状态机, 在主循环中调用 */
void Control_HandleInput(void);
/* 判断卡号是否在授权列表中 */
u8   Control_CardAuthorized(u8 *id);

#endif
