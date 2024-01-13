/*
Author: <Chuanyu> (skewcy@gmail.com)
header_dpdk.h (c) 2024
Desc: description
Created:  2024-01-02T19:11:40.625Z
*/

#ifndef HEADER_H
#define HEADER_H

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_timer.h>
#include <stdio.h>

#define RTE_ETHER_TYPE_RAW 0x1234

void setup_ethernet_header(struct rte_mbuf* pkt, struct rte_ether_addr* src_addr, struct rte_ether_addr* dst_addr, int vlan_enabled, int udp_ip_enabled, uint16_t vlan_id, uint16_t vlan_priority);

struct rte_ipv4_hdr* setup_ip_header(struct rte_mbuf* pkt, uint32_t src_ip, uint32_t dst_ip, size_t data_size);

struct rte_udp_hdr* setup_udp_header(struct rte_mbuf* pkt, uint16_t src_port, uint16_t dst_port, size_t data_size);

void setup_data_payload(struct rte_mbuf* pkt, const char* data, size_t data_size);

#endif  // HEADER_H
