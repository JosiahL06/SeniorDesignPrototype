# TO DO

## Website

Fix formatting of test view frame:
- Start test buttons should be moved below position data
- Make configuration tabs less prominent?

Move Logs to seperate frame
- Maybe on the left column, or make third column for logs

Simplify User/Clinician View tab
- Show position data in mm/cm, as in distance sternum is lifted?
- For setting degrees per step, user should select in terms of mm/cm instead
- Display position data graphically, plotted over time in reference to target goal

Add Graph frame
- Graph position data over time, with target goal as reference line

## Arduino Control Code

- 

## Motor Functions

- Change logic to move motors to absolute positon rather than relative (possibly by using encoder counts to track position from startup)

## BLE Functions

- Fix motor command ACK to send only when motors are finished moving, rather than immediately when command is received