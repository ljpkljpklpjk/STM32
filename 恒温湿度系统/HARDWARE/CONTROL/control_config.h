#ifndef CONTROL_CONFIG_H
#define CONTROL_CONFIG_H

#define CONTROL_SAMPLE_MS          2000UL
#define CONTROL_STALE_MS           6000UL

/* Mechanical relays: slow time-proportioning, NOT high-frequency PWM.
 * Verify these values against the relay and load specifications before use.
 */
#define CONTROL_WINDOW_MS          60000UL
#define CONTROL_MIN_ON_MS          5000UL
#define CONTROL_MIN_OFF_MS         5000UL
#define CONTROL_REVERSE_OFF_MS     5000UL
#define CONTROL_COLDFAN_MIN_DUTY   30

/* Starting gains only; output unit is percent, error in degC or %RH,
 * time in seconds. Both loops use signed output (-100..100).
 */
#define TEMP_PID_KP                12.0f
#define TEMP_PID_KI                0.08f
#define TEMP_PID_KD                3.0f
#define TEMP_PID_KP_ADJUST         6.0f
#define TEMP_PID_KI_ADJUST         0.06f
#define TEMP_PID_KD_ADJUST         2.0f
#define TEMP_PID_ERROR_RANGE       5.0f
#define TEMP_PID_RATE_RANGE        0.5f
#define TEMP_PID_INTEGRAL_ZONE     3.0f
#define TEMP_PID_DERIVATIVE_TAU     4.0f

#define HUMI_PID_KP                6.0f
#define HUMI_PID_KI                0.04f
#define HUMI_PID_KD                1.0f
#define HUMI_PID_KP_ADJUST         3.0f
#define HUMI_PID_KI_ADJUST         0.03f
#define HUMI_PID_KD_ADJUST         0.8f
#define HUMI_PID_ERROR_RANGE       10.0f
#define HUMI_PID_RATE_RANGE        1.0f
#define HUMI_PID_INTEGRAL_ZONE     6.0f
#define HUMI_PID_DERIVATIVE_TAU     4.0f

#if CONTROL_WINDOW_MS < (CONTROL_MIN_ON_MS + CONTROL_MIN_OFF_MS)
#error Control window must allow minimum ON and OFF times
#endif
#if CONTROL_SAMPLE_MS == 0 || CONTROL_STALE_MS <= CONTROL_SAMPLE_MS
#error Invalid sample or stale timeout
#endif

#endif
