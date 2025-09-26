#include "subsys/motion/planner.h"
#include "core/events.h"
#include <zephyr/logging/log.h>
#include <math.h>
LOG_MODULE_REGISTER(planner, LOG_LEVEL_INF);

// Forward declarations
static void planner_motion_work_handler(struct k_work *work);
static float calculate_move_time(position_t start, position_t end, float feed_rate_mm_s);
static float calculate_distance_3d(position_t start, position_t end);
static int execute_coordinated_move(planner_t *p, position_t target, float feed_rate_mm_s);

int planner_init(planner_t *p, const stepper_cfg_t *x_cfg, const stepper_cfg_t *y_cfg, const stepper_cfg_t *z_cfg)
{
    if (!p || !x_cfg || !y_cfg || !z_cfg) {
        return -EINVAL;
    }
    
    memset(p, 0, sizeof(planner_t));
    
    // Initialize stepper motors
    int rc = 0;
    rc |= stepper_init(&p->x, x_cfg);
    rc |= stepper_init(&p->y, y_cfg);
    rc |= stepper_init(&p->z, z_cfg);
    
    if (rc) {
        LOG_ERR("Failed to initialize stepper motors: %d", rc);
        return rc;
    }
    
    // Initialize planner state
    p->state = PLANNER_STATE_IDLE;
    p->homed = false;
    
    // Set default motion parameters
    p->max_feed_rate_mm_s = PLANNER_MAX_FEED_RATE_MM_S;
    p->current_feed_rate_mm_s = DEFAULT_FEED_RATE_MM_S;
    p->acceleration_mm_s2 = ACCELERATION_MM_S2;
    
    // Chess board parameters
    p->board_size_mm = CHESS_BOARD_SIZE_MM;
    p->square_size_mm = CHESS_SQUARE_SIZE_MM;
    p->safe_height_mm = SAFE_HEIGHT_MM;
    
    // Initialize synchronization
    k_mutex_init(&p->mutex);
    k_work_init_delayable(&p->motion_work, planner_motion_work_handler);
    
    LOG_INF("Motion planner initialized successfully");
    return 0;
}

int planner_enable(planner_t *p, bool enable)
{
    if (!p) {
        return -EINVAL;
    }
    
    int rc = 0;
    rc |= stepper_enable(&p->x, enable);
    rc |= stepper_enable(&p->y, enable);
    rc |= stepper_enable(&p->z, enable);
    
    if (rc == 0) {
        LOG_INF("Motion planner %s", enable ? "enabled" : "disabled");
    } else {
        LOG_ERR("Failed to %s motion planner: %d", enable ? "enable" : "disable", rc);
    }
    
    return rc;
}

int planner_home_all(planner_t *p)
{
    if (!p) {
        return -EINVAL;
    }
    
    LOG_INF("Starting homing sequence for all axes");
    
    k_mutex_lock(&p->mutex, K_FOREVER);
    p->state = PLANNER_STATE_HOMING;
    k_mutex_unlock(&p->mutex);
    
    // Home Z axis first (up), then Y, then X
    int rc = 0;
    
    rc |= stepper_home(&p->z);
    if (rc) {
        LOG_ERR("Failed to home Z axis: %d", rc);
        goto home_error;
    }
    
    rc |= stepper_home(&p->y);
    if (rc) {
        LOG_ERR("Failed to home Y axis: %d", rc);
        goto home_error;
    }
    
    rc |= stepper_home(&p->x);
    if (rc) {
        LOG_ERR("Failed to home X axis: %d", rc);
        goto home_error;
    }
    
    k_mutex_lock(&p->mutex, K_FOREVER);
    p->current_pos = (position_t){0, 0, 0};
    p->target_pos = p->current_pos;
    p->homed = true;
    p->state = PLANNER_STATE_IDLE;
    k_mutex_unlock(&p->mutex);
    
    LOG_INF("All axes homed successfully");
    
    // Post homing complete event
    event_t ev = {.type = EV_MOTION_DONE, .ts = k_uptime_get()};
    events_post(&ev, K_NO_WAIT);
    
    return 0;

home_error:
    k_mutex_lock(&p->mutex, K_FOREVER);
    p->state = PLANNER_STATE_ERROR;
    k_mutex_unlock(&p->mutex);
    return rc;
}

static float calculate_distance_3d(position_t start, position_t end)
{
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float dz = end.z - start.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

static float calculate_move_time(position_t start, position_t end, float feed_rate_mm_s)
{
    float distance = calculate_distance_3d(start, end);
    return distance / feed_rate_mm_s;
}

static int execute_coordinated_move(planner_t *p, position_t target, float feed_rate_mm_s)
{
    if (!p) {
        return -EINVAL;
    }
    
    float distance = calculate_distance_3d(p->current_pos, target);
    if (distance < 0.001f) {
        return 0; // Already at target
    }
    
    float move_time_s = distance / feed_rate_mm_s;
    
    // Calculate individual axis speeds to complete move in same time
    float dx = target.x - p->current_pos.x;
    float dy = target.y - p->current_pos.y;
    float dz = target.z - p->current_pos.z;
    
    float x_speed = (move_time_s > 0) ? fabsf(dx) / move_time_s : 0;
    float y_speed = (move_time_s > 0) ? fabsf(dy) / move_time_s : 0;
    float z_speed = (move_time_s > 0) ? fabsf(dz) / move_time_s : 0;
    
    // Start all axes simultaneously for coordinated motion
    int rc = 0;
    if (fabsf(dx) > 0.001f) {
        rc |= stepper_move_mm_async(&p->x, dx, x_speed);
    }
    if (fabsf(dy) > 0.001f) {
        rc |= stepper_move_mm_async(&p->y, dy, y_speed);
    }
    if (fabsf(dz) > 0.001f) {
        rc |= stepper_move_mm_async(&p->z, dz, z_speed);
    }
    
    if (rc == 0) {
        p->target_pos = target;
        p->total_moves++;
        p->total_distance_mm += distance;
        LOG_DBG("Coordinated move started: (%.2f,%.2f,%.2f) at %.1f mm/s", 
                target.x, target.y, target.z, feed_rate_mm_s);
    }
    
    return rc;
}

int planner_move_linear(planner_t *p, float x, float y, float z, float feed_rate_mm_s)
{
    if (!p) {
        return -EINVAL;
    }
    
    if (!p->homed) {
        LOG_ERR("Cannot move - planner not homed");
        return -EACCES;
    }
    
    k_mutex_lock(&p->mutex, K_FOREVER);
    
    if (p->state != PLANNER_STATE_IDLE) {
        k_mutex_unlock(&p->mutex);
        LOG_ERR("Cannot move - planner busy");
        return -EBUSY;
    }
    
    p->state = PLANNER_STATE_MOVING;
    
    position_t target = {x, y, z};
    int rc = execute_coordinated_move(p, target, feed_rate_mm_s);
    
    if (rc != 0) {
        p->state = PLANNER_STATE_IDLE;
    }
    
    k_mutex_unlock(&p->mutex);
    return rc;
}

int planner_move_to(planner_t *p, float x, float y, float z)
{
    return planner_move_linear(p, x, y, z, p->current_feed_rate_mm_s);
}

int planner_move_rapid(planner_t *p, float x, float y, float z)
{
    return planner_move_linear(p, x, y, z, RAPID_FEED_RATE_MM_S);
}

int planner_move_relative(planner_t *p, float dx, float dy, float dz)
{
    if (!p) {
        return -EINVAL;
    }
    
    return planner_move_to(p, p->current_pos.x + dx, p->current_pos.y + dy, p->current_pos.z + dz);
}

// Chess coordinate conversion functions
position_t chess_square_to_position(uint8_t file, uint8_t rank, float square_size_mm)
{
    position_t pos;
    pos.x = file * square_size_mm + square_size_mm / 2.0f;  // Center of square
    pos.y = rank * square_size_mm + square_size_mm / 2.0f;  // Center of square
    pos.z = 0.0f;  // Board level
    return pos;
}

void position_to_chess_square(position_t pos, uint8_t *file, uint8_t *rank, float square_size_mm)
{
    if (file) *file = (uint8_t)(pos.x / square_size_mm);
    if (rank) *rank = (uint8_t)(pos.y / square_size_mm);
}

bool is_valid_chess_square(uint8_t file, uint8_t rank)
{
    return (file < 8) && (rank < 8);
}

// Chess-specific movement functions
int planner_move_to_square(planner_t *p, uint8_t file, uint8_t rank)
{
    if (!p || !is_valid_chess_square(file, rank)) {
        return -EINVAL;
    }
    
    position_t target = chess_square_to_position(file, rank, p->square_size_mm);
    return planner_move_to(p, target.x, target.y, target.z);
}

int planner_move_to_square_safe(planner_t *p, uint8_t file, uint8_t rank)
{
    if (!p || !is_valid_chess_square(file, rank)) {
        return -EINVAL;
    }
    
    position_t target = chess_square_to_position(file, rank, p->square_size_mm);
    
    // Move to safe height first, then to square
    int rc = planner_move_to(p, p->current_pos.x, p->current_pos.y, p->safe_height_mm);
    if (rc == 0) {
        rc = planner_wait_for_completion(p, K_SECONDS(10));
        if (rc == 0) {
            rc = planner_move_to(p, target.x, target.y, p->safe_height_mm);
        }
    }
    
    return rc;
}

int planner_pickup_piece(planner_t *p, uint8_t file, uint8_t rank)
{
    if (!p || !is_valid_chess_square(file, rank)) {
        return -EINVAL;
    }
    
    // Move to safe position above square
    int rc = planner_move_to_square_safe(p, file, rank);
    if (rc != 0) return rc;
    
    rc = planner_wait_for_completion(p, K_SECONDS(10));
    if (rc != 0) return rc;
    
    // Lower to board level
    position_t target = chess_square_to_position(file, rank, p->square_size_mm);
    rc = planner_move_to(p, target.x, target.y, 0);
    if (rc != 0) return rc;
    
    rc = planner_wait_for_completion(p, K_SECONDS(5));
    if (rc != 0) return rc;
    
    // TODO: Activate electromagnet or gripper here
    
    // Lift piece to safe height
    rc = planner_move_to(p, target.x, target.y, p->safe_height_mm);
    
    LOG_INF("Picked up piece from %c%d", 'a' + file, rank + 1);
    return rc;
}

int planner_drop_piece(planner_t *p, uint8_t file, uint8_t rank)
{
    if (!p || !is_valid_chess_square(file, rank)) {
        return -EINVAL;
    }
    
    // Move to safe position above square
    int rc = planner_move_to_square_safe(p, file, rank);
    if (rc != 0) return rc;
    
    rc = planner_wait_for_completion(p, K_SECONDS(10));
    if (rc != 0) return rc;
    
    // Lower to board level
    position_t target = chess_square_to_position(file, rank, p->square_size_mm);
    rc = planner_move_to(p, target.x, target.y, 0);
    if (rc != 0) return rc;
    
    rc = planner_wait_for_completion(p, K_SECONDS(5));
    if (rc != 0) return rc;
    
    // TODO: Deactivate electromagnet or gripper here
    
    // Return to safe height
    rc = planner_move_to(p, target.x, target.y, p->safe_height_mm);
    
    LOG_INF("Dropped piece at %c%d", 'a' + file, rank + 1);
    return rc;
}

int planner_capture_sequence(planner_t *p, uint8_t from_file, uint8_t from_rank, 
                            uint8_t to_file, uint8_t to_rank)
{
    if (!p || !is_valid_chess_square(from_file, from_rank) || 
        !is_valid_chess_square(to_file, to_rank)) {
        return -EINVAL;
    }
    
    LOG_INF("Starting capture sequence: %c%d -> %c%d", 
            'a' + from_file, from_rank + 1, 'a' + to_file, to_rank + 1);
    
    // First, remove the captured piece (move to side of board)
    int rc = planner_pickup_piece(p, to_file, to_rank);
    if (rc != 0) return rc;
    
    // Move captured piece to side area (file 8+)
    position_t capture_area = {p->board_size_mm + 20.0f, to_rank * p->square_size_mm, 0};
    rc = planner_move_to(p, capture_area.x, capture_area.y, p->safe_height_mm);
    if (rc != 0) return rc;
    
    rc = planner_drop_piece(p, 8, to_rank); // Off-board position
    if (rc != 0) return rc;
    
    // Now move our piece
    rc = planner_pickup_piece(p, from_file, from_rank);
    if (rc != 0) return rc;
    
    rc = planner_drop_piece(p, to_file, to_rank);
    
    LOG_INF("Capture sequence completed");
    return rc;
}

int planner_wait_for_completion(planner_t *p, k_timeout_t timeout)
{
    if (!p) {
        return -EINVAL;
    }
    
    int64_t start_time = k_uptime_get();
    int64_t timeout_ms = timeout.ticks == K_FOREVER.ticks ? INT64_MAX : k_ticks_to_ms_floor64(timeout.ticks);
    
    while (planner_is_moving(p)) {
        if (k_uptime_get() - start_time > timeout_ms) {
            LOG_WRN("Motion timeout after %lld ms", k_uptime_get() - start_time);
            return -ETIMEDOUT;
        }
        k_msleep(10);
    }
    
    // Update current position
    k_mutex_lock(&p->mutex, K_FOREVER);
    p->current_pos.x = stepper_get_position(&p->x);
    p->current_pos.y = stepper_get_position(&p->y);  
    p->current_pos.z = stepper_get_position(&p->z);
    if (p->state == PLANNER_STATE_MOVING) {
        p->state = PLANNER_STATE_IDLE;
    }
    k_mutex_unlock(&p->mutex);
    
    return 0;
}

int planner_stop(planner_t *p)
{
    if (!p) {
        return -EINVAL;
    }
    
    int rc = 0;
    rc |= stepper_stop(&p->x);
    rc |= stepper_stop(&p->y);
    rc |= stepper_stop(&p->z);
    
    k_mutex_lock(&p->mutex, K_FOREVER);
    p->state = PLANNER_STATE_IDLE;
    k_mutex_unlock(&p->mutex);
    
    LOG_INF("Motion planner stopped");
    return rc;
}

int planner_emergency_stop(planner_t *p)
{
    if (!p) {
        return -EINVAL;
    }
    
    stepper_emergency_stop(&p->x);
    stepper_emergency_stop(&p->y);
    stepper_emergency_stop(&p->z);
    
    k_mutex_lock(&p->mutex, K_FOREVER);
    p->state = PLANNER_STATE_ERROR;
    k_mutex_unlock(&p->mutex);
    
    LOG_WRN("Emergency stop activated");
    return 0;
}

int planner_set_feed_rate(planner_t *p, float feed_rate_mm_s)
{
    if (!p || feed_rate_mm_s <= 0 || feed_rate_mm_s > p->max_feed_rate_mm_s) {
        return -EINVAL;
    }
    
    k_mutex_lock(&p->mutex, K_FOREVER);
    p->current_feed_rate_mm_s = feed_rate_mm_s;
    k_mutex_unlock(&p->mutex);
    
    return 0;
}

// Status functions
planner_state_t planner_get_state(planner_t *p)
{
    return p ? p->state : PLANNER_STATE_ERROR;
}

position_t planner_get_position(planner_t *p)
{
    if (!p) {
        return (position_t){0, 0, 0};
    }
    return p->current_pos;
}

bool planner_is_homed(planner_t *p)
{
    return p ? p->homed : false;
}

bool planner_is_moving(planner_t *p)
{
    if (!p) {
        return false;
    }
    
    return (p->state == PLANNER_STATE_MOVING || p->state == PLANNER_STATE_HOMING) ||
           stepper_is_moving(&p->x) || stepper_is_moving(&p->y) || stepper_is_moving(&p->z);
}

float planner_get_feed_rate(planner_t *p)
{
    return p ? p->current_feed_rate_mm_s : 0.0f;
}

static void planner_motion_work_handler(struct k_work *work)
{
    // Reserved for future motion queue implementation
}

// Motion queue functions (stubs for future implementation)
int planner_queue_motion(planner_t *p, position_t target, motion_type_t type, float feed_rate_mm_s)
{
    // TODO: Implement motion queue for complex sequences
    return -ENOTSUP;
}

int planner_clear_queue(planner_t *p)
{
    // TODO: Implement motion queue clearing
    return -ENOTSUP;
}

size_t planner_get_queue_size(planner_t *p)
{
    // TODO: Return actual queue size
    return 0;
}