#ifndef FUZZY_PID_H
#define FUZZY_PID_H

typedef struct
{
    float kp, ki, kd;          /* Base gains, time unit: seconds. */
    float kp_adjust, ki_adjust, kd_adjust; /* Max fuzzy gain corrections. */
    float error_range;         /* Physical error corresponding to fuzzy +/-3. */
    float rate_range;          /* Error rate corresponding to fuzzy +/-3. */
    float integral_zone;       /* Integrate only within this error magnitude. */
    float derivative_tau;      /* Measurement derivative low-pass time (s). */
} FuzzyPID_Config_t;

typedef struct
{
    const FuzzyPID_Config_t *config;
    float kp, ki, kd;          /* Current gains, useful in a debugger. */
    float integral;
    float last_error, last_measurement, derivative;
    float output;             /* Signed power demand, -100..100 percent. */
    unsigned char initialized;
} FuzzyPID_t;

void FuzzyPID_Init(FuzzyPID_t *pid, const FuzzyPID_Config_t *config);
void FuzzyPID_Reset(FuzzyPID_t *pid);
float FuzzyPID_Update(FuzzyPID_t *pid, float setpoint, float measurement, float dt);

#endif
