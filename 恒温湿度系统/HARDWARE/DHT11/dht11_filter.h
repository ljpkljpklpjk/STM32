#ifndef __DHT11_FILTER_H
#define __DHT11_FILTER_H
#include "sys.h"

/* Independent scalar Kalman filters, nominal sampling period 2 seconds.
 * Q: process variance per sample; R: measurement variance.
 * Temperature units: degC^2; humidity units: (%RH)^2.
 * Starting values require tuning from actual sensor measurements.
 */
#define DHT11_TEMP_Q    0.04f
#define DHT11_TEMP_R    1.0f
#define DHT11_HUMI_Q    0.25f
#define DHT11_HUMI_R    4.0f

typedef struct
{
    s16 temp_x10;
    s16 humi_x10;
    u8 valid;
    u8 err_cnt;
} DHT11_Filter_t;

void DHT11_Filter_Init(void);
/* One sample: 0 success; 1 communication/range failure, invalid immediately. */
u8 DHT11_Filter_Update(void);
u8 DHT11_Get_Temp(void);
u8 DHT11_Get_Humi(void);
/* Tenths are filtered estimates, not improved sensor accuracy. */
s16 DHT11_Get_Temp_x10(void);
s16 DHT11_Get_Humi_x10(void);
u8 DHT11_Data_Valid(void);

#endif
