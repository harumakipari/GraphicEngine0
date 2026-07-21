#ifndef MODEL_TYPE_INCLUDE
#define MODEL_TYPE_INCLUDE

static const int MATERIAL_DEFAULT = 0;
static const int MATERIAL_HAIR = 1;
static const int MATERIAL_FUR = 2;
static const int MATERIAL_SKIN = 3;
static const int MATERIAL_EYE = 4;
static const int MATERIAL_METALLIC = 5;

static const int OBJECT_DEFAULT = 0;
static const int OBJECT_PLAYER = 1;
static const int OBJECT_ENEMY = 2;
static const int OBJECT_STAGE = 3;
static const int OBJECT_NOT_SSR = 4;
static const int OBJECT_DOOR = 5;
static const int OBJECT_FURNITURE = 6;
static const int OBJECT_ENEMY_EYE = 7;
static const int OBJECT_NO_LIGHTING = 8;

static const int GBUFFER_FLAG_NORMAL = 0;
static const int GBUFFER_FLAG_SKY = 1;
static const int GBUFFER_FLAG_EMISSIVE = 2;

#endif