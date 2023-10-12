#include "cy_test.h"

int main(int argc, char **argv)
{
    rte_eal_init(argc, argv);
    uint8_t nb_ports = rte_eth_dev_count_avail();
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL",               /* name */
        256,                       /* # elements */
        0,                         /* cache size */
        0,                         /* application private area size */
        RTE_MBUF_DEFAULT_BUF_SIZE, /* data buffer size */
        rte_socket_id()            /* socket ID */
    );

    if (mbuf_pool == NULL)
    {
        printf("Could not create mbuf pool: %s, rte_errno: %d\n", rte_strerror(rte_errno), rte_errno);
        rte_exit(EXIT_FAILURE, "Exiting...\n");
    }

    // Port configuration and initialization
    struct rte_eth_conf port_conf = {0};
    rte_eth_dev_configure(0, 1, 1, &port_conf);
    rte_eth_rx_queue_setup(0, 0, 128, rte_eth_dev_socket_id(0), NULL, mbuf_pool);
    rte_eth_tx_queue_setup(0, 0, 128, rte_eth_dev_socket_id(0), NULL);
    rte_eth_dev_start(0);

    struct Message
    {
        char data[10];
    };
    struct ether_hdr *eth_hdr;
    struct Message obj = {{'H', 'e', 'l', 'l', 'o', '2', '0', '2', '3'}};
    struct Message *msg;

    struct rte_ether_addr s_addr = {{0x14, 0x02, 0xEC, 0x89, 0x8D, 0x24}};
    struct rte_ether_addr d_addr = {{0x14, 0x02, 0xEC, 0x89, 0xED, 0x54}};
    uint16_t ether_type = 0x0a00;

    // Fill in the packet data here
    while (true)
    {
        struct rte_mbuf *pkt = rte_pktmbuf_alloc(mbuf_pool);
        eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
        eth_hdr->d_addr = d_addr;
        eth_hdr->s_addr = s_addr;
        eth_hdr->ether_type = ether_type;
        msg = (struct Message *)(rte_pktmbuf_mtod(pkt, char *) + sizeof(struct ether_hdr));
        *msg = obj;
        int pkt_size = sizeof(struct Message) + sizeof(struct ether_hdr);
        pkt->data_len = pkt_size;
        pkt->pkt_len = pkt_size;

        uint16_t sent = rte_eth_tx_burst(0, 0, &pkt, 1);
        if (sent)
            printf("Packet sent\n");
        ;
        sleep(1);
    }

    return 0;
}
