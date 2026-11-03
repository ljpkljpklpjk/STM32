#include "dht11_filter.h"
#include "dht11.h"

typedef struct
{
    float x;
    float p;
} Kalman1D_t;

static DHT11_Filter_t g_dht;
static Kalman1D_t s_temp, s_humi;

/* Random-walk model: A=H=1. Q and R use physical units squared. */
static float Kalman_Update(Kalman1D_t *kf, float measurement, float q, float r)
{
    float gain;
    kf->p += q;
    gain = kf->p / (kf->p + r);
    kf->x += gain * (measurement - kf->x);
    kf->p *= (1.0f - gain);
    return kf->x;
}

void DHT11_Filter_Init(void)
{
    g_dht.temp_x10 = 0;
    g_dht.humi_x10 = 0;
    g_dht.valid = 0;
    g_dht.err_cnt = 0;
    s_temp.x = s_humi.x = 0.0f;
    s_temp.p = DHT11_TEMP_R;
    s_humi.p = DHT11_HUMI_R;
}

/* One transaction per sampling period; no rapid retries. */
u8 DHT11_Filter_Update(void)
{
    u8 t = 0, h = 0;

    if(DHT11_Read_Data(&t, &h) != 0 || t > 50 || h < 20 || h > 90)
    {
        if(g_dht.err_cnt < 255) g_dht.err_cnt++;
        /* Never drive the PID with an old measurement after a failed sample. */
        g_dht.valid = 0;
        return 1;
    }
    g_dht.err_cnt = 0;
    if(!g_dht.valid)
    {
        /* First good sample, including recovery after a sensor fault. */
        s_temp.x = (float)t;
        s_humi.x = (float)h;
        s_temp.p = DHT11_TEMP_R;
        s_humi.p = DHT11_HUMI_R;
    }
    else
    {
        Kalman_Update(&s_temp, (float)t, DHT11_TEMP_Q, DHT11_TEMP_R);
        Kalman_Update(&s_humi, (float)h, DHT11_HUMI_Q, DHT11_HUMI_R);
    }
    g_dht.temp_x10 = (s16)(s_temp.x * 10.0f + 0.5f);
    g_dht.humi_x10 = (s16)(s_humi.x * 10.0f + 0.5f);
    g_dht.valid = 1;
    return 0;
}

u8  DHT11_Get_Temp(void)     { return (u8)((g_dht.temp_x10 + 5) / 10); }
u8  DHT11_Get_Humi(void)     { return (u8)((g_dht.humi_x10 + 5) / 10); }
s16 DHT11_Get_Temp_x10(void) { return g_dht.temp_x10; }
s16 DHT11_Get_Humi_x10(void) { return g_dht.humi_x10; }
u8  DHT11_Data_Valid(void)   { return g_dht.valid; }
