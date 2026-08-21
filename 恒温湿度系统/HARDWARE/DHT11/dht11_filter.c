#include "dht11_filter.h"
#include "dht11.h"
#include "delay.h"

static DHT11_Filter_t g_dht = {0, 0, 0, 0};

//中值滤波窗口, 保存最近 DHT11_MED_N 次采样(每个采集周期一个点)
static u8 s_tbuf[DHT11_MED_N];
static u8 s_hbuf[DHT11_MED_N];
static u8 s_idx = 0;        //环形缓冲写指针
static u8 s_cnt = 0;        //缓冲中有效点数

//取数组中位数(在副本上排序, 不破坏原缓冲)
static u8 Median(const u8 *src, u8 n)
{
    u8 tmp[DHT11_MED_N];
    u8 i, j, t;

    for(i = 0; i < n; i++) tmp[i] = src[i];

    for(i = 0; i < n - 1; i++)
    {
        for(j = 0; j < n - 1 - i; j++)
        {
            if(tmp[j] > tmp[j + 1])
            {
                t = tmp[j];
                tmp[j] = tmp[j + 1];
                tmp[j + 1] = t;
            }
        }
    }
    return tmp[n / 2];
}

void DHT11_Filter_Init(void)
{
    g_dht.temp_x10 = 0;
    g_dht.humi_x10 = 0;
    g_dht.valid    = 0;
    g_dht.err_cnt  = 0;
    s_idx = 0;
    s_cnt = 0;
}

/*
 * 采集并滤波一次, 由主循环按 SAMPLE_PERIOD_MS(>1s) 的周期调用。
 *
 * DHT11 内部约1s才更新一次转换结果, 短时间内连读拿到的是同一个缓存值,
 * 所以中值窗口按"采集周期"跨周期铺开(窗口5点 ≈ 10s), 才能真正滤掉跳变点;
 * 单次采集内的重试只用于应对单总线通讯误码。
 */
u8 DHT11_Filter_Update(void)
{
    u8 t = 0, h = 0;
    u8 ok = 0;
    u8 retry;
    u8 tm, hm;

    //通讯误码时最多重试3次
    for(retry = 0; retry < 3; retry++)
    {
        if(DHT11_Read_Data(&t, &h) == 0)
        {
            //量程保护: DHT11 温度0~50℃, 湿度20%~90%RH, 明显越界的视为坏点
            if(t <= 60 && h >= 5 && h <= 95) { ok = 1; break; }
        }
        delay_ms(50);
    }

    if(!ok)
    {
        if(g_dht.err_cnt < 255) g_dht.err_cnt++;
        if(g_dht.err_cnt >= 3) g_dht.valid = 0;     //连续3个周期失败判定传感器异常
        return 1;
    }
    g_dht.err_cnt = 0;

    //压入中值窗口
    s_tbuf[s_idx] = t;
    s_hbuf[s_idx] = h;
    s_idx = (u8)((s_idx + 1) % DHT11_MED_N);
    if(s_cnt < DHT11_MED_N) s_cnt++;

    tm = Median(s_tbuf, s_cnt);
    hm = Median(s_hbuf, s_cnt);

    if(g_dht.valid == 0)            //第一次取得数据直接装载, 避免从0慢慢爬升
    {
        g_dht.temp_x10 = (s16)tm * 10;
        g_dht.humi_x10 = (s16)hm * 10;
        g_dht.valid = 1;
    }
    else                            //一阶滞后: y += alpha*(x - y)
    {
        g_dht.temp_x10 += (s16)(((s32)((s16)tm * 10 - g_dht.temp_x10) * DHT11_EMA_ALPHA) / 100);
        g_dht.humi_x10 += (s16)(((s32)((s16)hm * 10 - g_dht.humi_x10) * DHT11_EMA_ALPHA) / 100);
    }

    return 0;
}

u8  DHT11_Get_Temp(void)      { return (u8)((g_dht.temp_x10 + 5) / 10); }
u8  DHT11_Get_Humi(void)      { return (u8)((g_dht.humi_x10 + 5) / 10); }
s16 DHT11_Get_Temp_x10(void)  { return g_dht.temp_x10; }
s16 DHT11_Get_Humi_x10(void)  { return g_dht.humi_x10; }
u8  DHT11_Data_Valid(void)    { return g_dht.valid; }
