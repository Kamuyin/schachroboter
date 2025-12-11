#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

/**
 * @brief Initialize diagnostics MQTT handlers
 * 
 * Subscribes to diagnostic topics for manual motor control and testing:
 *   chess/diag/stepper/move   - Move a specific stepper motor
 *   chess/diag/stepper/stop   - Stop a specific or all stepper motors
 *   chess/diag/stepper/status - Query stepper motor status
 *   chess/diag/stepper/enable - Enable/disable stepper motors
 *   chess/diag/stepper/home   - Set current position as home (zero)
 *   chess/diag/servo/set      - Set servo angle
 *   chess/diag/servo/enable   - Enable/disable servo
 * 
 * Responses are published to:
 *   chess/diag/stepper/response
 *   chess/diag/servo/response
 * 
 * @return 0 on success, negative errno on failure
 */
int diagnostics_init(void);

#endif /* DIAGNOSTICS_H */
