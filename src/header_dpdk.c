/*
Author: <Chuanyu> (skewcy@gmail.com)
header_dpdk.c (c) 2024
Desc: description
Created:  2024-01-02T19:11:43.389Z
*/

#include "header_dpdk.h"

void setup_ethernet_header(struct rte_mbuf *pkt, struct rte_ether_addr *src_addr, struct rte_ether_addr *dst_addr,
                           int vlan_enabled, int udp_ip_enabled, uint16_t vlan_id, uint16_t vlan_priority) {
    struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *)rte_pktmbuf_append(pkt, sizeof(struct rte_ether_hdr));
    rte_ether_addr_copy(src_addr, &eth_hdr->src_addr);
    rte_ether_addr_copy(dst_addr, &eth_hdr->dst_addr);
    if (vlan_enabled) {
        eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
        struct rte_vlan_hdr *vlan_hdr = (struct rte_vlan_hdr *)rte_pktmbuf_append(pkt, sizeof(struct rte_vlan_hdr));
        vlan_hdr->vlan_tci = rte_cpu_to_be_16(vlan_id | (vlan_priority << 13));
        if (udp_ip_enabled) {
            vlan_hdr->eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
        } else {
            vlan_hdr->eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_RAW);
        }
    } else {
        if (udp_ip_enabled) {
            eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
        } else {
            eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_RAW);
        }
    }
}

struct rte_ipv4_hdr *setup_ip_header(struct rte_mbuf *pkt, uint32_t src_ip, uint32_t dst_ip, size_t data_size) {
    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)rte_pktmbuf_append(pkt, sizeof(struct rte_ipv4_hdr));
    ip_hdr->version_ihl = 0x45;
    ip_hdr->type_of_service = 0;
    ip_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + data_size);
    ip_hdr->packet_id = 0;
    ip_hdr->fragment_offset = 0;
    ip_hdr->time_to_live = 64;
    ip_hdr->next_proto_id = IPPROTO_UDP;
    ip_hdr->hdr_checksum = 0;  // Checksum will be calculated later
    ip_hdr->src_addr = rte_cpu_to_be_32(src_ip);
    ip_hdr->dst_addr = rte_cpu_to_be_32(dst_ip);
    return ip_hdr;
}

struct rte_udp_hdr *setup_udp_header(struct rte_mbuf *pkt, uint16_t src_port, uint16_t dst_port, size_t data_size) {
    struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)rte_pktmbuf_append(pkt, sizeof(struct rte_udp_hdr));
    udp_hdr->src_port = rte_cpu_to_be_16(src_port);
    udp_hdr->dst_port = rte_cpu_to_be_16(dst_port);
    udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct rte_udp_hdr) + data_size);
    udp_hdr->dgram_cksum = 0;  // Checksum can be 0 for UDP, or calculated later
    return udp_hdr;
}

void setup_data_payload(struct rte_mbuf *pkt, const char *data, size_t data_size) {
    char *payload = rte_pktmbuf_append(pkt, data_size);
    if (payload != NULL) {
        rte_memcpy(payload, data, data_size);
    }
}
