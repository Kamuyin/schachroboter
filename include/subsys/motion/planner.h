#pragma once
#include "hal/stepper.h"
#include "core/events.h"
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

// Motion planning constants
#define PLANNER_MAX_FEED_RATE_MM_S      100.0f    // Maximum feed rate
#define DEFAULT_FEED_RATE_MM_S  50.0f     // Default movement speed
#define RAPID_FEED_RATE_MM_S    80.0f     // Rapid positioning speed
#define ACCELERATION_MM_S2      500.0f    // Acceleration for smooth motion
#define JERK_MM_S3             1000.0f    // Jerk limitation

// Chess board dimensions (customizable)
#define CHESS_BOARD_SIZE_MM     200.0f    // Board edge length
#define CHESS_SQUARE_SIZE_MM    25.0f     // Individual square size
#define SAFE_HEIGHT_MM          10.0f     // Safe travel height above pieces

// Motion types
typedef enum {
    MOTION_TYPE_LINEAR,       // Straight line movement
    MOTION_TYPE_RAPID,        // Fast positioning (G0)
    MOTION_TYPE_INTERPOLATED, // Coordinated multi-axis
    MOTION_TYPE_PICKUP,       // Pick up piece sequence
    MOTION_TYPE_DROP,         // Drop piece sequence
    MOTION_TYPE_CAPTURE       // Capture piece sequence
} motion_type_t;

// Planner state
typedef enum {
    PLANNER_STATE_IDLE,
    PLANNER_STATE_HOMING,
    PLANNER_STATE_MOVING,
    PLANNER_STATE_ERROR
} planner_state_t;

// 3D position structure
typedef struct {
    float x, y, z;
} position_t;

// Motion block for trajectory planning
typedef struct motion_block {
    position_t start_pos;
    position_t end_pos;
    float feed_rate_mm_s;
    float acceleration_mm_s2;
    motion_type_t type;
    uint32_t duration_ms;
    struct motion_block *next;
} motion_block_t;

// Enhanced motion planner
typedef struct {
    // Stepper motors
    stepper_t x, y, z;
    
    // Current state
    planner_state_t state;
    position_t current_pos;
    position_t target_pos;
    bool homed;
    
    // Motion parameters
    float max_feed_rate_mm_s;
    float current_feed_rate_mm_s;
    float acceleration_mm_s2;
    
    // Chess-specific parameters
    float board_size_mm;
    float square_size_mm;
    float safe_height_mm;
    
    // Motion queue
    motion_block_t *motion_queue;
    motion_block_t *current_block;
    
    // Synchronization
    struct k_mutex mutex;
    struct k_work_delayable motion_work;
    
    // Statistics
    uint32_t total_moves;
    uint32_t total_distance_mm;
} planner_t;

// Core functions
int planner_init(planner_t *p, const stepper_cfg_t *x_cfg, const stepper_cfg_t *y_cfg, const stepper_cfg_t *z_cfg);
int planner_home_all(planner_t *p);
int planner_enable(planner_t *p, bool enable);

// Motion commands
int planner_move_to(planner_t *p, float x, float y, float z);
int planner_move_linear(planner_t *p, float x, float y, float z, float feed_rate_mm_s);
int planner_move_rapid(planner_t *p, float x, float y, float z);
int planner_move_relative(planner_t *p, float dx, float dy, float dz);

// Chess-specific movements
int planner_move_to_square(planner_t *p, uint8_t file, uint8_t rank);
int planner_move_to_square_safe(planner_t *p, uint8_t file, uint8_t rank);
int planner_pickup_piece(planner_t *p, uint8_t file, uint8_t rank);
int planner_drop_piece(planner_t *p, uint8_t file, uint8_t rank);
int planner_capture_sequence(planner_t *p, uint8_t from_file, uint8_t from_rank, 
                            uint8_t to_file, uint8_t to_rank);

// Motion control
int planner_stop(planner_t *p);
int planner_emergency_stop(planner_t *p);
int planner_wait_for_completion(planner_t *p, k_timeout_t timeout);
int planner_set_feed_rate(planner_t *p, float feed_rate_mm_s);

// Status and utility functions
planner_state_t planner_get_state(planner_t *p);
position_t planner_get_position(planner_t *p);
bool planner_is_homed(planner_t *p);
bool planner_is_moving(planner_t *p);
float planner_get_feed_rate(planner_t *p);

// Chess coordinate conversion
position_t chess_square_to_position(uint8_t file, uint8_t rank, float square_size_mm);
void position_to_chess_square(position_t pos, uint8_t *file, uint8_t *rank, float square_size_mm);
bool is_valid_chess_square(uint8_t file, uint8_t rank);

// Motion queue management
int planner_queue_motion(planner_t *p, position_t target, motion_type_t type, float feed_rate_mm_s);
int planner_clear_queue(planner_t *p);
size_t planner_get_queue_size(planner_t *p);

#ifdef __cplusplus
}
#endif