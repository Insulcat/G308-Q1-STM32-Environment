#ifndef Q5_CONTROL_H
#define Q5_CONTROL_H

#include <stdint.h>

typedef enum
{
  Q5_MODE_POTENTIOMETER = 0,
  Q5_MODE_MPU6050 = 1
} Q5_ControlMode_t;

typedef struct
{
  uint8_t mode;
  uint8_t pan_deg;
  uint8_t tilt_deg;
  uint8_t status;
} Q5_ControlState_t;

/* Status bits placed in every Bluetooth packet. */
#define Q5_STATUS_ADC_READY  (1u << 0)
#define Q5_STATUS_MPU_READY  (1u << 1)

void Q5_ControlInit(Q5_ControlState_t *state);
void Q5_ControlUpdate(Q5_ControlState_t *state);
void Q5_ControlSend(const Q5_ControlState_t *state);

#endif /* Q5_CONTROL_H */
