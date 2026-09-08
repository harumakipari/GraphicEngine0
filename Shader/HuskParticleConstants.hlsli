#ifndef HUSK_PARTICLE_CONSTANTS_INCLUDED
#define HUSK_PARTICLE_CONSTANTS_INCLUDED


cbuffer PARTICLE_CONSTANTS : register(b12)
{
    uint particle_count;
    float particle_size;
    float particle_option;
    float delta_time;
    float world_x_min;
    float world_x_max;
    float death_progress;
    float detach_speed;
    float gravity_;
    float lifetime;
    float rise_speed;
    float max_start_delay;
    float rise_speed_min_multiplier;
    float rise_speed_max_multiplier;
    float horizontal_random_speed;
    float fade_start_ratio;
    float lifetime_min_multiplier;
    float lifetime_max_multiplier;
    float display_ratio;
    float debug_normalized_x;
    float boundary_width;
    float boundary_emissive_strength;
    float detach_glow_duration;
    float detach_glow_strength;
    float3 body_color_multiplier;
    float body_brightness;
    float use_scene_color_capture;
    float scene_color_depth_threshold;
    float scene_color_capture_ready;
    float capture_padding;
};

#endif
