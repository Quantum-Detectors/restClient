#ifndef REST_DEFINITIONS_H
#define REST_DEFINITIONS_H

typedef enum
{
    REST_P_UNINIT,
    REST_P_BOOL,
    REST_P_INT,
    REST_P_UINT,
    REST_P_DOUBLE,
    REST_P_STRING,
    REST_P_ENUM,
    REST_P_COMMAND,
} rest_param_type_t;

typedef enum
{
    REST_ACC_RO,
    REST_ACC_RW,
    REST_ACC_WO
} rest_access_mode_t;

typedef struct
{
    bool exists;
    union
        {
            int valInt;
            double valDouble;
        };
} rest_min_max_t;

#endif
