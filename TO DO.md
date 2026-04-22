# TO DO

## Website

### Completed

- [x] Start test buttons moved below position data in Testing View
- [x] Configuration panels made less prominent (collapsible sections)
- [x] Logs moved to a separate frame/column
- [x] User View displays lift position in mm
- [x] User View uses mm inputs for actuation step and target lift
- [x] Position data is displayed graphically over time with a target reference line
- [x] Added graph frame for lift trend tracking

### Remaining

- [ ] Refine test-view layout styling as needed after user testing

## Arduino Control Code

### Completed

- [x] BLE command queueing and command handling in main loop
- [x] Motor position BLE notifications for both motor pairs
- [x] BLE metrics packet generation and notification during BT test mode
- [x] Data echo handling for Bluetooth throughput/error-rate testing
- [x] FreeRTOS-based asynchronous motor control task per motor pair

### Remaining

- [ ] Fine-tune encoder control accuracy at higher motor speeds

## Motor Functions

- [ ] Change logic to move motors to absolute position rather than relative (possibly by using encoder counts to track position from startup)

## BLE Functions

- [ ] Fix motor command ACK to send only when motors are finished moving, rather than immediately when command is received