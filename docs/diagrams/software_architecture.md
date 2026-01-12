# Software Architecture and Threading Diagram

This diagram illustrates the layered software architecture and threading model of the Schachroboter firmware.

## Architecture Overview

The firmware follows a **layered architecture** with clear separation of concerns:

1. **Application Layer** - Three concurrent RTOS threads handling high-level logic
2. **Domain Services Layer** - Business logic and coordination
3. **Driver Layer** - Hardware abstraction for peripherals
4. **HAL Layer** - Zephyr RTOS and STM32 hardware interfaces

## Threading Model

| Thread | Stack | Priority | Responsibility |
|--------|-------|----------|----------------|
| **Application Task** | 4 KB | 5 | Board scanning, event publishing, command handling |
| **MQTT Client Thread** | 4 KB | 5 | Network I/O, broker connection, message routing |
| **Robot Controller Task** | 2 KB | 5 | Stepper pulse generation, position tracking, homing |

All threads run at equal priority (cooperative scheduling) with preemption.

## Diagram

```mermaid
block-beta
  columns 3
  
  %% Application Layer
  block:appLayer:3
    columns 3
    space:1
    appTitle["Application Layer"]
    space:1
    
    app["Application Task\n(Board Events, Commands)"]
    mqtt["MQTT Client Thread\n(Network I/O)"]
    robot["Robot Controller Task\n(Motion Control)"]
  end
  
  space:3
  
  %% Domain Services Layer
  block:domainLayer:3
    columns 3
    space:1
    domainTitle["Domain Services Layer"]
    space:1
    
    boardMgr["Board Manager\n(Move Detection)"]
    mqttSub["MQTT Subscriptions\n(Command Routing)"]
    stepperMgr["Stepper Manager\n(Motor Coordination)"]
  end
  
  space:3
  
  %% Driver Layer
  block:driverLayer:3
    columns 4
    space:1
    driverTitle["Driver Layer"]
    space:2
    
    boardDrv["Board Driver\n(Matrix Scan)"]
    stepperDrv["Stepper Motor\n(Pulse Gen)"]
    servoDrv["Servo Motor\n(PWM)"]
    limitDrv["Limit Switch\n(ISR)"]
  end
  
  space:3
  
  %% HAL Layer
  block:halLayer:3
    columns 4
    space
    halTitle["Zephyr HAL / Hardware"]
    space:2
    
    gpio["GPIO"]
    network["Ethernet\nTCP/IP Stack"]
    timers["Timers\nCounters"]
    interrupts["NVIC\nInterrupts"]
  end
  
  %% Connections
  app --> boardMgr
  mqtt --> mqttSub
  robot --> stepperMgr
  
  boardMgr --> boardDrv
  stepperMgr --> stepperDrv
  stepperMgr --> servoDrv
  stepperMgr --> limitDrv

  classDef thread fill:#4a90d9,stroke:#2c5a8c,color:#fff
  classDef service fill:#5cb85c,stroke:#3d7a3d,color:#fff
  classDef driver fill:#f0ad4e,stroke:#c78c2c,color:#000
  classDef hal fill:#d9534f,stroke:#a32c29,color:#fff
  classDef title fill:none,stroke:none,color:#333
  
  class app,mqtt,robot thread
  class boardMgr,mqttSub,stepperMgr service
  class boardDrv,stepperDrv,servoDrv,limitDrv driver
  class gpio,network,timers,interrupts hal
```

## Layer Descriptions

### Application Layer (Blue)
- **Application Task**: Orchestrates board scanning at 100ms intervals, detects chess moves, publishes state changes via MQTT, handles incoming robot commands
- **MQTT Client Thread**: Maintains persistent connection to broker, manages subscriptions, handles publish/subscribe message flow
- **Robot Controller Task**: Executes motion commands, manages stepper motor timing, coordinates multi-axis movements

### Domain Services Layer (Green)
- **Board Manager**: Tracks 64-bit occupancy mask, detects move patterns (simple moves, castling, captures)
- **MQTT Subscriptions**: Routes incoming messages to appropriate handlers based on topic
- **Stepper Manager**: Coordinates 5 motors (X, Y1, Y2, Z, Gripper), handles synchronized Y-axis movement

### Driver Layer (Orange)
- **Board Driver**: Implements row-by-row matrix scanning for reed switch array
- **Stepper Motor**: Software-based pulse generation with position tracking
- **Servo Motor**: PWM control for gripper/auxiliary actuators
- **Limit Switch**: GPIO interrupt handlers for homing and safety stops

### Hardware Abstraction Layer (Red)
- **Zephyr RTOS APIs**: GPIO, Network stack, Timer subsystems
- **STM32F767 Peripherals**: Direct hardware access where needed (NVIC, etc.)

## Data Flow

1. **Board State Flow**: Reed switches → Board Driver → Board Manager → MQTT → Host Application
2. **Command Flow**: Host → MQTT → Application → Robot Controller → Stepper Motors
3. **Safety Flow**: Limit Switch ISR → Emergency Stop → Motor Position Reset
