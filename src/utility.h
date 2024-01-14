/*
Author: <Chuanyu> (skewcy@gmail.com)
utility.h (c) 2024
Desc: description
Created:  2024-01-04T15:18:53.134Z
*/

#ifndef SRC_UTILITY_H_
#define SRC_UTILITY_H_

#include <stdint.h>
#include <stdio.h>

char* mac_addr_to_string(uint8_t* mac_addr);
uint8_t* mac_addr_from_string(char* mac_str);
uint32_t ip_addr_from_string(char* ip_str);

#endif  // UTILITY_H