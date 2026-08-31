#ifndef Q5_RECEIVER_H
#define Q5_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint8_t mode;
  uint8_t pan_deg;
  uint8_t tilt_deg;
  uint8_t status;
  uint8_t sequence;
  uint32_t last_rx_tick;
} Q5_ServoCommand_t;

void Q5_ReceiverInit(Q5_ServoCommand_t *command);
bool Q5_ReceiverParseByte(uint8_t byte, Q5_ServoCommand_t *command);
void Q5_ServoApply(const Q5_ServoCommand_t *command);

#endif /* Q5_RECEIVER_H */
