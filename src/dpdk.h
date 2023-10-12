#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif


#define TX_RING_SIZE 128
#define RX_RING_SIZE 128
#define NUM_MBUFS TX_RING_SIZE + RX_RING_SIZE - 1 // -1 as DPDK recommended
// How to set mempool size: https://inbox.dpdk.org/users/20221206082457.3574df15@hermes.local/T/

/**
 * Ethernet header: Contains the destination address, source address
 * and frame type.
 */
struct ether_hdr
{
    struct rte_ether_addr d_addr; /**< Destination address. */
    struct rte_ether_addr s_addr; /**< Source address. */
    uint16_t vlan_tpid;           /**< 0x8100 for VLAN */
    uint16_t vlan_tci;            /** [3pcp] + [1 (drop 0)] + [12 vlan id] **/
    uint16_t ether_type;          /**< Frame type. */
} __attribute__((__packed__));

/**
 * IPv4 Header
 */
struct ipv4_hdr
{
    uint8_t version_ihl;      /**< version and header length */
    uint8_t type_of_service;  /**< type of service */
    uint16_t total_length;    /**< length of packet */
    uint16_t packet_id;       /**< packet ID */
    uint16_t fragment_offset; /**< fragmentation offset */
    uint8_t time_to_live;     /**< time to live */
    uint8_t next_proto_id;    /**< protocol ID */
    uint16_t hdr_checksum;    /**< header checksum */
    uint32_t src_addr;        /**< source address */
    uint32_t dst_addr;        /**< destination address */
} __attribute__((__packed__));

/**
 * UDP Header
 */
struct udp_hdr
{
    uint16_t src_port;    /**< UDP source port. */
    uint16_t dst_port;    /**< UDP destination port. */
    uint16_t dgram_len;   /**< UDP datagram length */
    uint16_t dgram_cksum; /**< UDP datagram checksum */
} __attribute__((__packed__));


int setup_device(int port_id, int rxq, int txq, struct rte_mempool *mbuf_pool);
int setup_memory(int port_id, struct rte_mempool *mbuf_pool);
int send_single(int port_id, int vlan, int pcp, struct rte_ether_addr address, char* msg, int length, struct rte_mempool *mbuf_pool);
int sche_single(int port_id, int vlan, int pcp, struct rte_ether_addr, uint64_t txtime, rte_mbuf *mbuf, struct Message* msg);