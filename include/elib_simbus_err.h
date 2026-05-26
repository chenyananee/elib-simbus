/* elib_simbus_err.h - SimBus Error Codes */

#ifndef ELIB_SIMBUS_ERR_H
#define ELIB_SIMBUS_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ELIB_SIMBUS_OK = 0,
    ELIB_SIMBUS_ERR_INVALID_PARAM,
    ELIB_SIMBUS_ERR_NOT_INITIALIZED,
    ELIB_SIMBUS_ERR_EXCEED_MAX,
    ELIB_SIMBUS_ERR_NACK,
    ELIB_SIMBUS_ERR_TIMEOUT,
} elib_simbus_err_t;

#ifdef __cplusplus
}
#endif

#endif /* ELIB_SIMBUS_ERR_H */
