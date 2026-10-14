#ifndef CLEVO_EC_H
#define CLEVO_EC_H
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

int init();
int set_fan_speed(int duty, int index);
int get_rpm(int index);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // !CLEVO_EC_H


