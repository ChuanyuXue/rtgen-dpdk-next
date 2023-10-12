#include "dpdk.h"

int setup_device(int port_id, int rxq, int txq, struct rte_mempool *mbuf_pool){
    struct rte_eth_conf port_conf = {0};
    rte_eth_dev_configure(port_id, rxq, txq, &port_conf);
    for (int i = 0; i < rxq; i++){
        rte_eth_rx_queue_setup(port_id, i, RX_RING_SIZE, rte_eth_dev_socket_id(0), NULL, mbuf_pool);
    }
    for (int i = 0; i < txq; i++){
        rte_eth_tx_queue_setup(port_id, i, TX_RING_SIZE, rte_eth_dev_socket_id(0), NULL);
    }
    rte_eth_dev_start(port_id);
    return 0;
}

int setup_memory(int port_id, struct rte_mempool *mbuf_pool){
    mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL",               /* name */
        NUM_MBUFS,                       /* # elements */
        0,                         /* cache size */
        0,                         /* application private area size */
        RTE_MBUF_DEFAULT_BUF_SIZE, /* data buffer size */
        rte_socket_id()            /* socket ID */
    );
    return 0;
}

int send_single(int port_id, int vlan, int pcp, struct rte_ether_addr address, char* msg, int length, struct rte_mempool *mbuf_pool){
    // Build Ethernet header
    struct rte_ether_addr* s_addr;
    rte_eth_macaddr_get(port_id, s_addr);
    uint16_t vlan_tpid = 0x8100;
    uint16_t vlan_tci = 0;
    // Set the VLAN ID (bits 0-11)
    vlan_tci |= (vlan & 0x0FFF);
    // Set the PCP (bits 12-14)
    vlan_tci |= ((pcp & 0x07) << 13);
    uint16_t ether_type = 0x0abc;

    struct rte_mbuf *pkt = rte_pktmbuf_alloc(mbuf_pool);

    struct ether_hdr *eth_hdr;
    {
        eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
        eth_hdr->d_addr = address;
        eth_hdr->s_addr = *s_addr;
        eth_hdr->vlan_tpid = vlan_tpid;
        eth_hdr->vlan_tci = vlan_tci;
        eth_hdr->ether_type = ether_type;
    };

    
    {   
        pkt->data_len = length + sizeof(struct ether_hdr);
        pkt->pkt_len = length + sizeof(struct ether_hdr);
    };
    
    

    return 0;
}


// int setup_receiver(int port_id);
// int setup_memory(int port_id);
// int send_single(int port_id, int vlan, int pcp, struct rte_ether_addr, struct Message* msg);
// int sche_single(int port_id, int vlan, int pcp, struct rte_ether_addr, uint64_t txtime, rte_mbuf *mbuf, struct Message* msg);