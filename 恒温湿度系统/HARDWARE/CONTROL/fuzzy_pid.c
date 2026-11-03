#include "fuzzy_pid.h"

/* Rows: error; columns: error rate. NB NM NS ZO PS PM PB = -3..3.
 * Singleton consequents are interpolated with triangular memberships.
 */
static const signed char s_kp[7][7] =
{
    { 3, 3, 2, 2, 1, 0, 0},
    { 3, 3, 2, 1, 1, 0,-1},
    { 2, 2, 2, 1, 0,-1,-1},
    { 2, 2, 1, 0,-1,-2,-2},
    { 1, 1, 0,-1,-1,-2,-2},
    { 1, 0,-1,-2,-2,-2,-3},
    { 0, 0,-2,-2,-2,-3,-3}
};
static const signed char s_ki[7][7] =
{
    {-3,-3,-2,-2,-1, 0, 0},
    {-3,-3,-2,-1,-1, 0, 0},
    {-3,-2,-1,-1, 0, 1, 1},
    {-2,-2,-1, 0, 1, 2, 2},
    {-2,-1, 0, 1, 1, 2, 3},
    { 0, 0, 1, 1, 2, 3, 3},
    { 0, 0, 1, 2, 2, 3, 3}
};
static const signed char s_kd[7][7] =
{
    { 1,-1,-3,-3,-3,-2, 1},
    { 1,-1,-3,-2,-2,-1, 0},
    { 0,-1,-2,-2,-1,-1, 0},
    { 0,-1,-1,-1,-1,-1, 0},
    { 0, 0, 0, 0, 0, 0, 0},
    { 3,-1, 1, 1, 1, 1, 3},
    { 3, 2, 2, 2, 1, 1, 3}
};

static float Limit(float x, float low, float high)
{
    if(x < low) return low;
    if(x > high) return high;
    return x;
}

static float Abs(float x) { return x < 0.0f ? -x : x; }

static void Membership(float x, unsigned char *index, float *fraction)
{
    float shifted = Limit(x, -3.0f, 3.0f) + 3.0f;
    *index = (unsigned char)shifted;
    if(*index >= 6) *index = 5;
    *fraction = shifted - (float)*index;
}

static float Infer(const signed char rules[7][7], unsigned char e,
                   unsigned char ec, float ef, float ecf)
{
    return ((1.0f-ef) * ((1.0f-ecf)*rules[e][ec] + ecf*rules[e][ec+1])
            + ef * ((1.0f-ecf)*rules[e+1][ec] + ecf*rules[e+1][ec+1])) / 3.0f;
}

void FuzzyPID_Reset(FuzzyPID_t *pid)
{
    pid->kp = pid->config->kp;
    pid->ki = pid->config->ki;
    pid->kd = pid->config->kd;
    pid->integral = 0.0f;
    pid->last_error = 0.0f;
    pid->last_measurement = 0.0f;
    pid->derivative = 0.0f;
    pid->output = 0.0f;
    pid->initialized = 0;
}

void FuzzyPID_Init(FuzzyPID_t *pid, const FuzzyPID_Config_t *config)
{
    pid->config = config;
    FuzzyPID_Reset(pid);
}

float FuzzyPID_Update(FuzzyPID_t *pid, float setpoint, float measurement, float dt)
{
    const FuzzyPID_Config_t *c = pid->config;
    float error, rate = 0.0f, ef, ecf, candidate, provisional;
    unsigned char e, ec;

    if(dt <= 0.0f) return pid->output;
    error = setpoint - measurement;
    if(pid->initialized)
    {
        rate = (error - pid->last_error) / dt;
        /* Derivative on measurement avoids a setpoint derivative kick. */
        pid->derivative += dt / (c->derivative_tau + dt)
            * ((measurement - pid->last_measurement) / dt - pid->derivative);
    }
    Membership(error * 3.0f / c->error_range, &e, &ef);
    Membership(rate * 3.0f / c->rate_range, &ec, &ecf);
    /* Corrections are relative to base gains, never accumulated each sample. */
    pid->kp = Limit(c->kp + c->kp_adjust * Infer(s_kp,e,ec,ef,ecf), 0.0f, c->kp+c->kp_adjust);
    pid->ki = Limit(c->ki + c->ki_adjust * Infer(s_ki,e,ec,ef,ecf), 0.0f, c->ki+c->ki_adjust);
    pid->kd = Limit(c->kd + c->kd_adjust * Infer(s_kd,e,ec,ef,ecf), 0.0f, c->kd+c->kd_adjust);

    if(Abs(error) > c->integral_zone) pid->integral = 0.0f;
    candidate = pid->integral;
    if(Abs(error) <= c->integral_zone)
        candidate = Limit(candidate + pid->ki * error * dt, -100.0f, 100.0f);
    provisional = pid->kp * error + candidate - pid->kd * pid->derivative;
    /* Freeze integration into saturation; permit it to unwind in reverse. */
    if((provisional <= 100.0f || error < 0.0f) &&
       (provisional >= -100.0f || error > 0.0f)) pid->integral = candidate;
    pid->output = Limit(pid->kp * error + pid->integral
                        - pid->kd * pid->derivative, -100.0f, 100.0f);
    pid->last_error = error;
    pid->last_measurement = measurement;
    pid->initialized = 1;
    return pid->output;
}
