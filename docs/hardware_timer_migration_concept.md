# Hardware Timer-Based Stepper Pulse Generation - Migration Concept

## Executive Summary

This document outlines a comprehensive strategy for migrating from software-based GPIO bit-banging to hardware timer-based PWM pulse generation for TB6600 stepper motor drivers on the STM32F767ZI platform.

**Migration Benefits:**
- ✅ Deterministic, jitter-free pulse timing independent of CPU load
- ✅ Zero CPU overhead during motion (no polling loops)
- ✅ Higher maximum step rates possible (10+ kHz vs current ~1 kHz)
- ✅ Improved motion smoothness and precision
- ✅ Frees CPU for other real-time tasks

**Migration Challenges:**
- ⚠️ Hardware pin remapping required (PWM-capable pins)
- ⚠️ More complex position tracking (requires interrupt or counter reading)
- ⚠️ Synchronization of dual Y-axis motors needs careful design
- ⚠️ Emergency stop implementation requires rethinking

---

## 1. Current Architecture Analysis

### Current Pin Assignments
| Motor    | PULSE Pin | DIR Pin  | ENABLE Pin | Notes |
|----------|-----------|----------|------------|-------|
| X-Axis   | PD15      | PF14     | PE9        |       |
| Y1-Axis  | PF5       | PF4      | PE8        |       |
| Y2-Axis  | PF10      | PE7      | PD14       |       |
| Z-Axis   | PD5       | PD6      | PD7        |       |
| Gripper  | PC6       | PC7      | PC8        |       |

### Current Software Flow
```
robot_controller_task() [2048 stack, THREAD_PRIORITY]
    └─> while(1) {
            stepper_manager_update_all()
                └─> stepper_motor_update(motor)
                    └─> if (now_us() >= next_step_time) {
                            gpio_pin_set(pulse, 1)
                            k_busy_wait(5µs)
                            gpio_pin_set(pulse, 0)
                            position++
                            next_step_time += step_delay_us
                        }
            k_sleep(K_USEC(100))  // assumed
        }
```

**Current Limitations:**
- Update rate limited by thread scheduling (≤10 kHz theoretical, ~1 kHz practical)
- `k_busy_wait()` blocks CPU for 5µs per step × 5 motors = 25µs blocked per step cycle
- Jitter from thread preemption affects motion smoothness
- High CPU usage during multi-axis moves

---

## 2. Proposed Hardware Timer Architecture

### 2.1 STM32F767 Timer Resources

The STM32F767ZI provides 14 timers with PWM capabilities:

| Timer | Type | Channels | Max Freq | Available for Steppers |
|-------|------|----------|----------|------------------------|
| TIM1  | Advanced | 4 + 3N | 216 MHz | ✅ |
| TIM2  | General  | 4 | 108 MHz | ✅ |
| TIM3  | General  | 4 | 108 MHz | ✅ |
| TIM4  | General  | 4 | 108 MHz | ✅ |
| TIM5  | General  | 4 | 108 MHz | ✅ |
| TIM8  | Advanced | 4 + 3N | 216 MHz | ✅ |
| TIM9-14 | General | 2-4 | 108 MHz | ✅ |

**Strategy:** Use 5 timer channels (one per motor) from available timers.

### 2.2 Pin Remapping Requirements

**STM32F767 PWM-Capable Pins Analysis:**

We need to remap PULSE pins to timer-capable pins. Example mapping:

| Motor    | New PULSE Pin | Timer | Channel | Original DIR | Original ENABLE |
|----------|---------------|-------|---------|--------------|-----------------|
| X-Axis   | **PE11**      | TIM1_CH2 | CH2  | PF14 (keep) | PE9 (keep) |
| Y1-Axis  | **PE5**       | TIM9_CH1 | CH1  | PF4 (keep)  | PE8 (keep) |
| Y2-Axis  | **PE6**       | TIM9_CH2 | CH2  | PE7 (keep)  | PD14 (keep) |
| Z-Axis   | **PD12**      | TIM4_CH1 | CH1  | PD6 (keep)  | PD7 (keep) |
| Gripper  | **PC6**       | TIM3_CH1 or TIM8_CH1 | CH1 | PC7 (keep) | PC8 (keep) |

**Important Notes:** 
- PC6 is already assigned and happens to be TIM3_CH1 or TIM8_CH1 capable. Other pins need rewiring.
- Y1 and Y2 use **different pins** (PE5 vs PE6) but **same timer** (TIM9) for hardware synchronization
- "Same timer, different channel" means shared counter but independent output pins

### 2.3 Architecture Design

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  robot_controller_move_to() / robot_controller_home()       │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│               Stepper Motor Manager (New)                   │
│  - Calculates step counts and speeds                        │
│  - Configures PWM frequency (step rate)                     │
│  - Sets DIR pins via GPIO                                   │
│  - Tracks target positions                                  │
│  - Registers PWM completion callbacks                       │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│            Zephyr PWM API (drivers/pwm)                     │
│  pwm_set_cycles() - Configure period and pulse width        │
│  pwm_get_cycles() - Read current configuration              │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│          STM32 Timer Hardware (TIM1-14)                     │
│  - Generates PWM pulses autonomously                        │
│  - Counter interrupt on update (optional)                   │
│  - DMA support (advanced)                                   │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Implementation Strategy

### 3.1 Phase 1: Hardware Setup & Device Tree

**Step 1: Enable PWM in prj.conf**
```conf
# Enable PWM subsystem
CONFIG_PWM=y  # Already enabled

# Enable specific timer instances
CONFIG_PWM_STM32=y
```

**Step 2: Define Timer PWM Nodes in Device Tree**

Create `nucleo_f767zi.overlay` additions:

```dts
&timers1 {
    status = "okay";
    
    pwm1: pwm {
        status = "okay";
        pinctrl-0 = <&tim1_ch2_pe11>;  // X-axis pulse
        pinctrl-names = "default";
    };
};

&timers9 {
    status = "okay";
    
    pwm9: pwm {
        status = "okay";
        pinctrl-0 = <&tim9_ch1_pe5    // Y1-axis pulse
                     &tim9_ch2_pe6>;  // Y2-axis pulse
        pinctrl-names = "default";
    };
};

&timers4 {
    status = "okay";
    
    pwm4: pwm {
        status = "okay";
        pinctrl-0 = <&tim4_ch1_pd12>;  // Z-axis pulse
        pinctrl-names = "default";
    };
};

&timers3 {
    status = "okay";
    
    pwm3: pwm {
        status = "okay";
        pinctrl-0 = <&tim3_ch1_pc6>;   // Gripper pulse
        pinctrl-names = "default";
    };
};
```

**Step 3: Update Stepper DT Nodes**

```dts
stepper_x: stepper-x {
    compatible = "custom,stepper";
    pwm = <&pwm1 2>;  // PWM device, channel 2
    dir-gpios = <&gpiof 14 GPIO_ACTIVE_HIGH>;
    enable-gpios = <&gpioe 9 GPIO_ACTIVE_HIGH>;
};

stepper_y1: stepper-y1 {
    compatible = "custom,stepper";
    pwm = <&pwm9 1>;  // PWM device, channel 1
    dir-gpios = <&gpiof 4 GPIO_ACTIVE_HIGH>;
    enable-gpios = <&gpioe 8 GPIO_ACTIVE_HIGH>;
};

stepper_y2: stepper-y2 {
    compatible = "custom,stepper";
    pwm = <&pwm9 2>;  // PWM device, channel 2 (same timer for sync)
    dir-gpios = <&gpioe 7 GPIO_ACTIVE_HIGH>;
    enable-gpios = <&gpiod 14 GPIO_ACTIVE_HIGH>;
};

stepper_z: stepper-z {
    compatible = "custom,stepper";
    pwm = <&pwm4 1>;
    dir-gpios = <&gpiod 6 GPIO_ACTIVE_HIGH>;
    enable-gpios = <&gpiod 7 GPIO_ACTIVE_HIGH>;
};

stepper_gripper: stepper-gripper {
    compatible = "custom,stepper";
    pwm = <&pwm3 1>;
    dir-gpios = <&gpioc 7 GPIO_ACTIVE_HIGH>;
    enable-gpios = <&gpioc 8 GPIO_ACTIVE_HIGH>;
};
```

### 3.2 Phase 2: Update Device Tree Bindings

Create/update `zephyr/dts/bindings/custom/custom,stepper.yaml`:

```yaml
description: Stepper motor with PWM pulse generation
compatible: "custom,stepper"

properties:
  pwm:
    type: phandle-array
    required: true
    description: |
      PWM phandle and channel for step pulse generation.
      Format: <&pwm_device channel>
      
  dir-gpios:
    type: phandle-array
    required: true
    description: GPIO for direction control (1 = CW, 0 = CCW)
    
  enable-gpios:
    type: phandle-array
    required: true
    description: GPIO for driver enable (active low on TB6600)
    
  dir-inverted:
    type: boolean
    description: If present, inverts the direction signal
    
  steps-per-mm:
    type: int
    description: Steps per millimeter (for units conversion)
    
  max-speed-hz:
    type: int
    default: 2000
    description: Maximum step rate in Hz
```

### 3.3 Phase 3: Rewrite stepper_motor.c Driver

**New Data Structure:**

```c
struct stepper_motor {
    // Hardware interfaces
    const struct device *pwm_dev;
    uint32_t pwm_channel;
    const struct device *dir_port;
    uint32_t dir_pin;
    const struct device *enable_port;
    uint32_t enable_pin;
    
    // Motion state
    int32_t current_position;    // Steps
    int32_t target_position;     // Steps
    uint32_t current_freq_hz;    // Current PWM frequency
    
    // Configuration
    uint32_t pwm_period_ns;      // PWM period in nanoseconds
    uint32_t pwm_pulse_ns;       // Pulse width in nanoseconds (50% duty for square wave)
    bool dir_inverted;
    
    // State management
    stepper_state_t state;
    stepper_direction_t direction;
    bool enabled;
    
    // Position tracking
    struct k_timer position_timer;  // For counting steps via timer interrupts
    volatile int32_t steps_completed;
    
    // Callbacks
    stepper_move_complete_callback_t callback;
};
```

**Key Functions to Reimplement:**

```c
int stepper_motor_move_steps(stepper_motor_t *motor, int32_t steps, 
                              uint32_t step_delay_us)
{
    if (!motor || !motor->enabled) {
        return -EINVAL;
    }
    
    if (steps == 0) {
        return 0;
    }
    
    // Set direction
    motor->direction = (steps > 0) ? STEPPER_DIR_CW : STEPPER_DIR_CCW;
    gpio_pin_set(motor->dir_port, motor->dir_pin, 
                 motor->direction ^ motor->dir_inverted);
    
    // Calculate PWM frequency from step delay
    uint32_t freq_hz = 1000000 / step_delay_us;  // Convert µs to Hz
    
    // Set PWM period to match step rate
    // For 1000 Hz: period = 1,000,000 ns = 1 ms
    uint32_t period_ns = 1000000000 / freq_hz;
    uint32_t pulse_ns = period_ns / 2;  // 50% duty cycle
    
    int ret = pwm_set_cycles(motor->pwm_dev, motor->pwm_channel,
                             PWM_NSEC(period_ns), PWM_NSEC(pulse_ns), 0);
    if (ret < 0) {
        LOG_ERR("Failed to set PWM: %d", ret);
        return ret;
    }
    
    motor->target_position = motor->current_position + steps;
    motor->steps_completed = 0;
    motor->state = STEPPER_STATE_MOVING;
    
    // Start position tracking timer (interrupt-based or polling)
    k_timer_start(&motor->position_timer, 
                  K_USEC(step_delay_us), 
                  K_USEC(step_delay_us));
    
    return 0;
}

void stepper_motor_emergency_stop(stepper_motor_t *motor)
{
    if (!motor) {
        return;
    }
    
    // Stop PWM generation immediately
    pwm_set_cycles(motor->pwm_dev, motor->pwm_channel, 0, 0, 0);
    
    // Stop position tracking
    k_timer_stop(&motor->position_timer);
    
    motor->target_position = motor->current_position;
    motor->state = STEPPER_STATE_IDLE;
}

// Position tracking timer callback
static void position_timer_callback(struct k_timer *timer)
{
    stepper_motor_t *motor = k_timer_user_data_get(timer);
    
    // Increment/decrement position based on direction
    if (motor->direction == STEPPER_DIR_CW) {
        motor->current_position++;
    } else {
        motor->current_position--;
    }
    
    motor->steps_completed++;
    
    // Check if target reached
    if (motor->current_position == motor->target_position) {
        stepper_motor_stop(motor);
        
        if (motor->callback) {
            motor->callback(motor);
        }
    }
}
```

### 3.4 Phase 4: Dual Y-Axis Synchronization

**Challenge:** Y1 and Y2 motors must stay perfectly synchronized.

**Solution 1: Same Timer, Different Channels (RECOMMENDED)**
- Use TIM9_CH1 for Y1 and TIM9_CH2 for Y2
- **Important:** "Same timer" means the motors share the same hardware counter (TIM9), but use **different channels and different physical pins**
  - Y1: TIM9 Channel 1 → Pin **PE5**
  - Y2: TIM9 Channel 2 → Pin **PE6**
- Both channels share the same timer counter, so pulses are generated at exactly the same time
- Hardware-guaranteed synchronization (zero drift, physically impossible to desynchronize)
- Configure both channels to same frequency simultaneously
- Each motor still has its own independent output pin for PULSE signal

```c
int stepper_motor_move_steps_sync(stepper_motor_t *motor_a, 
                                   stepper_motor_t *motor_b,
                                   int32_t steps, uint32_t step_delay_us)
{
    // Both motors must use the same timer base
    __ASSERT(motor_a->pwm_dev == motor_b->pwm_dev, 
             "Y-axis motors must share timer");
    
    // Set direction on both
    stepper_direction_t dir = (steps > 0) ? STEPPER_DIR_CW : STEPPER_DIR_CCW;
    gpio_pin_set(motor_a->dir_port, motor_a->dir_pin, dir ^ motor_a->dir_inverted);
    gpio_pin_set(motor_b->dir_port, motor_b->dir_pin, dir ^ motor_b->dir_inverted);
    
    // Calculate PWM parameters
    uint32_t freq_hz = 1000000 / step_delay_us;
    uint32_t period_ns = 1000000000 / freq_hz;
    uint32_t pulse_ns = period_ns / 2;
    
    // Configure both channels simultaneously
    pwm_set_cycles(motor_a->pwm_dev, motor_a->pwm_channel,
                   PWM_NSEC(period_ns), PWM_NSEC(pulse_ns), 0);
    pwm_set_cycles(motor_b->pwm_dev, motor_b->pwm_channel,
                   PWM_NSEC(period_ns), PWM_NSEC(pulse_ns), 0);
    
    // Update state
    motor_a->target_position = motor_a->current_position + steps;
    motor_b->target_position = motor_b->current_position + steps;
    motor_a->state = motor_b->state = STEPPER_STATE_MOVING;
    
    // Use single timer for both (they'll stay in sync)
    k_timer_start(&motor_a->position_timer, 
                  K_USEC(step_delay_us), 
                  K_USEC(step_delay_us));
    
    return 0;
}
```

**Solution 2: Master-Slave Configuration**
- One timer triggers both motors
- Y1 is master, Y2 follows
- Less flexible but simpler

### 3.5 Phase 5: Position Tracking Methods

**Option A: Software Timer Polling (SIMPLE)**
- Use `k_timer` to increment position counter
- Timer period matches step rate
- Pro: Simple, no hardware dependency
- Con: Timer drift over long moves, interrupt overhead

**Option B: Hardware Counter Reading (ADVANCED)**
- Read TIMx->CNT register to get pulse count
- Compare against target count
- Pro: Hardware-accurate, no drift
- Con: Requires direct register access, complex for Zephyr

**Option C: Update Event Interrupt (OPTIMAL)**
- Configure TIM_DIER.UIE (Update Interrupt Enable)
- ISR fires on each PWM period completion
- Increment position in ISR
- Pro: Perfectly synchronized, hardware-accurate
- Con: Higher interrupt rate, need ISR handler

**Recommended: Option C (Update Event Interrupt)**

```c
// In stepper_motor_init()
IRQ_CONNECT(TIM1_UP_TIM10_IRQn, 2, tim1_update_isr, motor, 0);
irq_enable(TIM1_UP_TIM10_IRQn);

static void tim1_update_isr(void *arg)
{
    stepper_motor_t *motor = (stepper_motor_t *)arg;
    
    // Clear interrupt flag
    LL_TIM_ClearFlag_UPDATE(TIM1);
    
    // Update position
    if (motor->state == STEPPER_STATE_MOVING) {
        if (motor->direction == STEPPER_DIR_CW) {
            motor->current_position++;
        } else {
            motor->current_position--;
        }
        
        // Check if done
        if (motor->current_position == motor->target_position) {
            // Stop PWM
            pwm_set_cycles(motor->pwm_dev, motor->pwm_channel, 0, 0, 0);
            motor->state = STEPPER_STATE_IDLE;
            
            // Schedule callback (can't call directly from ISR)
            if (motor->callback) {
                k_work_submit(&motor->callback_work);
            }
        }
    }
}
```

---

## 4. Migration Roadmap

### Phase 1: Preparation (Week 1)
- [ ] Document current motor pin assignments
- [ ] Identify available PWM-capable pins on PCB
- [ ] Test individual timer PWM output with oscilloscope
- [ ] Verify TB6600 operation with timer-generated pulses

### Phase 2: Device Tree & Build System (Week 1-2)
- [ ] Add timer PWM nodes to device tree overlay
- [ ] Update stepper device tree bindings
- [ ] Enable PWM in Kconfig
- [ ] Test build (won't run yet, just compile check)

### Phase 3: Single Motor Prototype (Week 2-3)
- [ ] Rewrite stepper_motor.c for PWM API
- [ ] Implement single motor movement (X-axis test)
- [ ] Add position tracking (start with Option A - software timer)
- [ ] Verify motion accuracy with test patterns
- [ ] Measure CPU usage improvement

### Phase 4: Multi-Motor Support (Week 3-4)
- [ ] Extend to all 5 motors
- [ ] Implement dual Y-axis synchronization
- [ ] Add emergency stop functionality
- [ ] Test coordinated multi-axis moves

### Phase 5: Advanced Features (Week 4-5)
- [ ] Implement hardware counter-based position tracking (Option C)
- [ ] Add acceleration/deceleration profiles (S-curve)
- [ ] Optimize interrupt handlers
- [ ] Performance benchmarking

### Phase 6: Integration & Testing (Week 5-6)
- [ ] Integration with robot_controller
- [ ] Integration with limit switches (homing)
- [ ] Full system testing
- [ ] Fallback/error handling
- [ ] Documentation update

---

## 5. Risk Analysis & Mitigation

### Risk 1: Pin Reassignment Requires Hardware Changes
**Impact:** HIGH  
**Probability:** CERTAIN  
**Mitigation:**
- Create pin mapping document before PCB changes
- Use breadboard/jumper wires for prototyping
- Consider PCB revision or wire rework
- Alternative: Use available PWM pins even if less optimal routing

### Risk 2: Y-Axis Synchronization Issues
**Impact:** HIGH (mechanical damage possible)  
**Probability:** MEDIUM  
**Mitigation:**
- Use same timer for both Y motors
- Implement synchronization verification
- Add mechanical coupling check routine
- Emergency stop if position mismatch detected

### Risk 3: Position Tracking Accuracy
**Impact:** MEDIUM  
**Probability:** LOW  
**Mitigation:**
- Multiple position tracking implementations
- Periodic position verification
- Encoder feedback (future enhancement)
- Software position validation

### Risk 4: Interrupt Overhead at High Step Rates
**Impact:** MEDIUM  
**Probability:** MEDIUM  
**Mitigation:**
- Optimize ISR handlers (keep minimal)
- Use DMA for advanced scenarios
- Profile interrupt timing
- Adjust priorities if needed

### Risk 5: Emergency Stop Latency
**Impact:** HIGH  
**Probability:** LOW  
**Mitigation:**
- Direct PWM disable in limit switch ISR
- No RTOS calls in critical path
- Test response time < 100µs
- Hardware disable as backup

---

## 6. Performance Comparison

### Current Software Implementation
| Metric | Value |
|--------|-------|
| Max step rate (single motor) | ~1000 Hz |
| Max step rate (5 motors) | ~500 Hz |
| CPU usage (idle) | ~5% |
| CPU usage (5 motors moving) | ~30-40% |
| Timing jitter | ±50-200µs |
| ISR latency impact | Yes, affects all motors |

### Projected Hardware Timer Implementation
| Metric | Value |
|--------|-------|
| Max step rate (single motor) | 20,000+ Hz |
| Max step rate (5 motors) | 20,000+ Hz per motor |
| CPU usage (idle) | <1% |
| CPU usage (5 motors moving) | <5% |
| Timing jitter | ±1µs (hardware precise) |
| ISR latency impact | Independent per motor |

**Expected Improvements:**
- 20× increase in maximum speed capability
- 80-90% reduction in CPU usage during motion
- 50-200× improvement in timing precision
- Complete elimination of motion smoothness issues
- Enables complex motion profiles (acceleration/deceleration)

---

## 7. Code Structure Changes

### New File Organization
```
src/drivers/peripherals/
├── stepper_motor.c          # Rewritten for PWM
├── stepper_motor_pwm.c      # PWM-specific implementation (NEW)
└── stepper_motor_legacy.c   # Old GPIO implementation (deprecated)

include/
├── stepper_motor.h          # Public API (unchanged interface)
└── stepper_motor_pwm.h      # PWM implementation details (NEW)

zephyr/dts/bindings/custom/
└── custom,stepper.yaml      # Updated with PWM properties
```

### API Compatibility Strategy
**Goal:** Maintain existing API so upper layers don't need changes.

```c
// Public API remains the same
int stepper_motor_move_steps(stepper_motor_t *motor, int32_t steps, 
                              uint32_t step_delay_us);

// Internal implementation switches from GPIO to PWM
// - Old: gpio_pin_set() + k_busy_wait()
// - New: pwm_set_cycles()
```

---

## 8. Testing Strategy

### Unit Tests
1. **Single PWM Channel Test**
   - Generate 1 kHz square wave
   - Verify with oscilloscope
   - Measure frequency accuracy

2. **Frequency Range Test**
   - Test 100 Hz to 10 kHz range
   - Verify TB6600 responds correctly
   - Check for missed steps

3. **Position Tracking Test**
   - Move 1000 steps forward
   - Verify position counter = 1000
   - Move 1000 steps back
   - Verify position counter = 0

### Integration Tests
1. **Multi-Axis Coordination**
   - Move X and Z simultaneously
   - Verify independent operation
   - Check CPU usage

2. **Y-Axis Synchronization**
   - Move Y1 and Y2 together
   - Monitor position difference
   - Should remain < 1 step difference

3. **Emergency Stop**
   - Trigger limit switch during motion
   - Measure stop latency
   - Verify position is set to zero

### System Tests
1. **Chess Move Simulation**
   - Execute full pick-and-place sequence
   - Verify accuracy
   - Repeat 100 times

2. **Homing Sequence**
   - Home all axes
   - Verify return to zero
   - Check repeatability

3. **Stress Test**
   - Continuous motion for 1 hour
   - Monitor for position drift
   - Check system stability

---

## 9. Rollback Plan

If hardware timer implementation causes critical issues:

1. **Keep Legacy Code**
   - Maintain `stepper_motor_legacy.c` with old implementation
   - Use Kconfig option to select implementation
   ```conf
   config STEPPER_USE_HARDWARE_PWM
       bool "Use hardware timers for stepper control"
       default n
   ```

2. **Runtime Fallback**
   - Detect PWM initialization failure
   - Automatically fall back to GPIO mode
   - Log warning message

3. **Per-Motor Selection**
   - Allow mixed mode (some PWM, some GPIO)
   - Useful during migration or if pins unavailable

---

## 10. Future Enhancements

Once hardware PWM is stable:

1. **Acceleration Profiles**
   - Linear acceleration/deceleration
   - S-curve motion profiles
   - Reduces mechanical stress

2. **DMA-Based Pulse Sequences**
   - Pre-program complex motion patterns
   - Zero CPU involvement
   - For repetitive chess moves

3. **Encoder Feedback**
   - Close position loop
   - Detect and correct missed steps
   - Ultimate accuracy

4. **Multi-Axis Interpolation**
   - Coordinated moves along paths
   - Straight lines, arcs
   - For smooth piece capture movements

---

## 11. Conclusion

The migration from software to hardware timer-based pulse generation is a **significant architectural improvement** that will:

- Dramatically reduce CPU load
- Enable much higher motion speeds
- Improve timing precision by 2-3 orders of magnitude
- Provide foundation for advanced motion control features

**Recommended Approach:**
1. Start with single-motor prototype (X-axis)
2. Validate with existing application
3. Gradually migrate other motors
4. Keep legacy code as fallback
5. Document lessons learned

**Critical Success Factors:**
- Careful pin remapping planning
- Robust Y-axis synchronization
- Thorough testing at each phase
- Maintain API compatibility

**Estimated Effort:** 4-6 weeks for full migration and testing

**Priority:** MEDIUM-HIGH (not blocking current functionality, but significant improvement)
