#include <stdio.h>
#include <rte_common.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_timer.h>

/* [TODO]: Design a header format to store udp_header, ip_header, and ethernet header together
    This speed up the process because we don't change the header during runtime
*/
struct rtgen_hdr
{
    struct rte_ether_hdr *eth_hdr;
    struct rte_vlan_hdr *vlan_hdr;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;
};

/* Interface to be implemented
    - port_id & queue_id
    - header structure pointer -> udp_header, ip_header, ethernet_header, vlan_header
    - payload structure -> a pointer to the payload
*/
int send_single(int port_id, struct rtgen_hdr* pheader, char *msg, uint16_t msg_len);
int sche_single(int port_id, struct rtgen_hdr* pheader, char *msg, uint16_t msg_len, uint64_t time);