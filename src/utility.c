/*
Author: <Chuanyu> (skewcy@gmail.com)
utility.c (c) 2024
Desc: description
Created:  2024-01-04T15:18:47.621Z
*/

#include "utility.h"

#include <arpa/inet.h>

char *rtgen_mac_addr_to_string(uint8_t *mac_addr) {
    static char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return mac_str;
}

uint8_t *rtgen_mac_addr_from_string(char *mac_str) {
    static uint8_t mac_addr[6];
    sscanf(mac_str, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
           &mac_addr[0], &mac_addr[1], &mac_addr[2], &mac_addr[3], &mac_addr[4], &mac_addr[5]);
    return mac_addr;
}

uint32_t rtgen_ip_addr_from_string(char *ip_str) {
    int ip_addr[4];
    sscanf(ip_str, "%d.%d.%d.%d", &ip_addr[0], &ip_addr[1], &ip_addr[2], &ip_addr[3]);
    return (uint32_t)((ip_addr[0] << 24) | (ip_addr[1] << 16) | (ip_addr[2] << 8) | ip_addr[3]);
}
