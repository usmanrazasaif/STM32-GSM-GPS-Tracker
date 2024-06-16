# STM32-Based Vehicle Tracker

Our STM32-based vehicle tracker is a robust and reliable solution designed to monitor and report vehicle locations using the STM32 Nucleo-767ZI board, uBlox NEO-6M GPS module, and SIM800L GSM module.

## Key Features

- **Real-Time Tracking:** The device continuously tracks the vehicle's location and sends updates to the Ubidots server every 30 seconds via HTTP POST using GSM, ensuring real-time monitoring.
- **Location on Demand:** By sending an SMS to the device, users can receive the current location instantly, providing quick access to vehicle whereabouts.
- **Efficient Task Management:** Utilizes RTOS for parallel task execution and semaphores for task blocking, ensuring efficient and reliable operation.
- **Seamless Integration:** Easy to integrate with existing systems and supports various applications such as fleet management, stolen vehicle recovery, and asset tracking.

This advanced vehicle tracker leverages the power of the STM32 platform and modern communication technologies to deliver a high-performance tracking solution for diverse automotive needs.

Parts Used:
NUCLEO-767ZI (STM32F676ZI)
GSM Module: SIM800L
GPS Module: ublox Neo 6M
