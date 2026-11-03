#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "platform_stub.h"

/* Compile the actual implementations; white-box access enables timing tests. */
#include "../HARDWARE/CONTROL/control.c"
#include "../HARDWARE/DHT11/dht11_filter.c"

static u32 test_tick;
static u8 heat, cool, fan, humidifier, cold_fan, beep;
static u8 raw_temp, raw_humi, read_error;
static unsigned int read_count;
static u8 keys[2], card_present;
static s16 deltas[2];

u32 Tick_Get(void) { return test_tick; }
u32 Tick_Elapsed(u32 start) { return (u32)(test_tick - start); }
void Heater_Set(u8 on) { heat = on; assert(!(heat && cool)); }
void Cooler_Set(u8 on) { cool = on; assert(!(heat && cool)); }
void Fan_Set(u8 on) { fan = on; assert(!(fan && humidifier)); }
void Humidifier_Set(u8 on) { humidifier = on; assert(!(fan && humidifier)); }
void ColdFan_SetDuty(u8 duty) { assert(duty <= 100); cold_fan = duty; }
void Beep_Set(u8 on) { beep = on; }
void Actuator_AllOff(void)
{
    heat = cool = fan = humidifier = cold_fan = beep = 0;
}
u8 DHT11_Read_Data(u8 *t, u8 *h)
{
    read_count++;
    *t = raw_temp;
    *h = raw_humi;
    return read_error;
}
u8 Encoder_Get_Key(u8 ch) { u8 k = keys[ch]; keys[ch] = 0; return k; }
s16 Encoder_Get_Delta(u8 ch) { s16 d = deltas[ch]; deltas[ch] = 0; return d; }
void RC522_Check(void) { }
u8 RC522_ReadCardID(u8 *id)
{
    if(!card_present) return 0;
    card_present = 0;
    id[0] = 0x41; id[1] = 0xA4; id[2] = 0x26; id[3] = 0x07;
    return 1;
}

static void Reset(void)
{
    Actuator_AllOff();
    test_tick = 0;
    keys[0] = keys[1] = card_present = 0;
    deltas[0] = deltas[1] = 0;
    read_error = 0;
    read_count = 0;
    Control_Init();
    DHT11_Filter_Init();
}

static void TestKalman(void)
{
    unsigned int i;
    s16 low = 1000, high = 0;
    Reset();
    assert(!DHT11_Data_Valid());
    raw_temp = 25; raw_humi = 50;
    assert(DHT11_Filter_Update() == 0);
    assert(DHT11_Get_Temp_x10() == 250 && DHT11_Get_Humi_x10() == 500);
    raw_temp = 35; raw_humi = 70;
    assert(DHT11_Filter_Update() == 0);
    assert(DHT11_Get_Temp_x10() == 301 && DHT11_Get_Humi_x10() == 603);
    for(i = 0; i < 500; i++) assert(!DHT11_Filter_Update());
    assert(DHT11_Get_Temp_x10() == 350 && DHT11_Get_Humi_x10() == 700);
    for(i = 0; i < 500; i++)
    {
        raw_temp = (i & 1) ? 24 : 26;
        assert(!DHT11_Filter_Update());
        if(i > 100)
        {
            if(DHT11_Get_Temp_x10() < low) low = DHT11_Get_Temp_x10();
            if(DHT11_Get_Temp_x10() > high) high = DHT11_Get_Temp_x10();
        }
    }
    assert(high - low < 20); /* Raw signal span is 2 degC. */
    read_error = 1;
    i = read_count;
    assert(DHT11_Filter_Update() == 1 && !DHT11_Data_Valid());
    assert(read_count == i + 1);
    for(i = 0; i < 300; i++) DHT11_Filter_Update();
    assert(g_dht.err_cnt == 255);
    read_error = 0; raw_temp = 40; raw_humi = 80;
    assert(!DHT11_Filter_Update());
    assert(DHT11_Get_Temp_x10() == 400 && DHT11_Get_Humi_x10() == 800);
    raw_temp = 51; assert(DHT11_Filter_Update() == 1);
    raw_temp = 25; raw_humi = 19; assert(DHT11_Filter_Update() == 1);
    raw_humi = 91; assert(DHT11_Filter_Update() == 1);
    raw_temp = 0; raw_humi = 20; assert(!DHT11_Filter_Update());
    raw_temp = 50; raw_humi = 90; assert(!DHT11_Filter_Update());
    puts("PASS Kalman initialization, step, noise, range, fault and recovery");
}

static void TestPID(void)
{
    FuzzyPID_t pid;
    FuzzyPID_Config_t derivative_only = {0,0,5, 0,0,0, 5,1,10,0};
    FuzzyPID_Config_t saturated = {100,1,0, 0,0,0, 5,1,10,0};
    unsigned int i;
    float kp;
    FuzzyPID_Init(&pid, &s_temp_config);
    assert(FuzzyPID_Update(&pid,25,20,2) > 0);
    FuzzyPID_Reset(&pid);
    assert(FuzzyPID_Update(&pid,25,35,2) < 0);
    FuzzyPID_Reset(&pid);
    assert(FuzzyPID_Update(&pid,25,25,2) == 0);
    FuzzyPID_Reset(&pid);
    FuzzyPID_Update(&pid,25,22,2);
    kp = pid.kp;
    for(i = 0; i < 1000; i++) FuzzyPID_Update(&pid,25,22,2);
    assert(fabs(pid.kp - kp) < 0.0001);
    assert(pid.integral <= 100 && pid.output <= 100);
    for(i = 0; i < 1000; i++)
    {
        FuzzyPID_Update(&pid,25,(float)(i % 51),2);
        assert(pid.output >= -100 && pid.output <= 100); /* Also rejects NaN. */
        assert(pid.kp >= 0 && pid.kp <= TEMP_PID_KP + TEMP_PID_KP_ADJUST);
        assert(pid.ki >= 0 && pid.ki <= TEMP_PID_KI + TEMP_PID_KI_ADJUST);
        assert(pid.kd >= 0 && pid.kd <= TEMP_PID_KD + TEMP_PID_KD_ADJUST);
    }
    FuzzyPID_Init(&pid, &derivative_only);
    assert(FuzzyPID_Update(&pid,25,20,2) == 0);
    assert(FuzzyPID_Update(&pid,35,20,2) == 0); /* No setpoint D kick. */
    assert(FuzzyPID_Update(&pid,35,21,2) < 0);
    kp = pid.output;
    assert(FuzzyPID_Update(&pid,35,50,0) == kp);
    FuzzyPID_Init(&pid, &saturated);
    for(i = 0; i < 1000; i++) assert(FuzzyPID_Update(&pid,25,23,2) == 100);
    assert(pid.integral == 0); /* No windup while saturated. */
    FuzzyPID_Reset(&pid);
    assert(pid.output == 0 && pid.integral == 0 && !pid.initialized);
    puts("PASS fuzzy gain bounds, output direction, anti-windup and D kick");
}

static void TestRelayTiming(void)
{
    RelayPair_t pair;
    u32 t, on_time = 0;
    Pair_Init(&pair, 0);
    Pair_Apply(&pair,100,4999); assert(pair.active == 0);
    Pair_Apply(&pair,100,5000); assert(pair.active == 1);
    Pair_Apply(&pair,-100,6000); assert(pair.active == 1);
    Pair_Apply(&pair,-100,9999); assert(pair.active == 1);
    Pair_Apply(&pair,-100,10000); assert(pair.active == 0);
    Pair_Apply(&pair,-100,14999); assert(pair.active == 0);
    Pair_Apply(&pair,-100,15000); assert(pair.active == -1);
    Pair_Stop(&pair,15001); assert(pair.active == 0); /* Fault overrides min ON. */
    Pair_Apply(&pair,-100,20000); assert(pair.active == 0);
    Pair_Apply(&pair,-100,20001); assert(pair.active == -1);

    Pair_Init(&pair, 0);
    for(t = 0; t < 180000; t += 10)
    {
        Pair_Apply(&pair,30,t);
        if(t >= 120000 && pair.active) on_time += 10;
    }
    assert(on_time == 18000); /* 30% of a settled 60 s window. */
    Pair_Init(&pair,0);
    for(t = 0; t < 120000; t += 100)
    {
        Pair_Apply(&pair,1,t);
        assert(!pair.active); /* Tiny duty does not create minimum-power bias. */
    }
    Pair_Init(&pair, UINT32_MAX - 2000);
    Pair_Apply(&pair,100,2998); assert(!pair.active);
    Pair_Apply(&pair,100,2999); assert(pair.active == 1);
    puts("PASS relay duty, minimum ON/OFF, reversal dead time and tick rollover");
}

static void TestSystem(void)
{
    u32 t;
    float integral;
    Reset();
    for(t = 0; t <= 6000; t += 100)
    {
        test_tick = t;
        if(t % CONTROL_SAMPLE_MS == 0) Control_Update(200,220,1);
        Control_ApplyOutputs();
    }
    assert(heat && !cool);
    integral = s_temp_pid.integral;
    Control_Update(200,220,1);
    assert(s_temp_pid.integral == integral); /* Same tick is not a new sample. */
    Control_Update(200,220,0);
    assert(!heat && !cool && !fan && !humidifier && !cold_fan);
    assert(s_temp_pid.integral == 0 && s_humi_pid.integral == 0);
    assert(!g_sys.temp_output_x10 && !g_sys.humi_output_x10);
    test_tick = 6500;
    Control_Update(200,220,1);
    assert(!heat); /* Recovery cannot bypass relay minimum OFF time. */
    test_tick = 12500;
    Control_ApplyOutputs();
    assert(!heat && !s_control_valid); /* Stale sample protection. */
    test_tick = 13000;
    Control_Update(501,250,1); assert(!s_control_valid);

    Reset();
    for(t = 0; t <= 6000; t += 100)
    {
        test_tick = t;
        if(t % CONTROL_SAMPLE_MS == 0) Control_Update(450,600,1);
        Control_ApplyOutputs();
    }
    assert(cool && !heat && fan && cold_fan >= CONTROL_COLDFAN_MIN_DUTY);
    keys[ENC_TEMP] = 1;
    Control_HandleInput();
    assert(g_sys.state == ST_WAIT_CARD && cold_fan == 0);
    Control_ApplyOutputs(); assert(cold_fan == 0);
    card_present = 1;
    Control_HandleInput();
    assert(g_sys.state == ST_ADJ_TEMP && cold_fan > 0);
    deltas[ENC_TEMP] = 1;
    Control_HandleInput();
    assert(g_sys.temp_set == TEMP_DEFAULT + 1);
    test_tick = 8000;
    Control_Update((s16)g_sys.temp_set * 10,250,1);
    assert(s_temp_pid.derivative == 0 && s_temp_pid.integral == 0);

    g_sys.temp_set = 25;
    Control_Alarm(350,250,1); assert(!beep);
    Control_Alarm(351,250,1); assert(beep);
    Control_Alarm(250,351,1); assert(beep);
    Control_Alarm(250,250,0); assert(!beep);
    puts("PASS GPIO interlocks, sensor stop/recovery, stale data, RFID and alarms");
}

int main(void)
{
    TestKalman();
    TestPID();
    TestRelayTiming();
    TestSystem();
    puts("All host control tests passed. Hardware tuning is still required.");
    return 0;
}
