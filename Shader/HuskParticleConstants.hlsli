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
    float particle_padding;
};

#endif
