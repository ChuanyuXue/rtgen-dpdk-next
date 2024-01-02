#include "udp.h"

struct dev_info dev_info_list[MAX_AVAILABLE_PORTS];

char *get_mac_addr(int port_id) {
    static char mac_str[18];
    struct rte_ether_addr *mac_addr;
    rte_eth_macaddr_get(port_id, mac_addr);
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr->addr_bytes[0], mac_addr->addr_bytes[1],
             mac_addr->addr_bytes[2], mac_addr->addr_bytes[3],
             mac_addr->addr_bytes[4], mac_addr->addr_bytes[5]);
    return mac_str;
}

int setup_EAL() {
    char *argv[] = {""};
    int ret = rte_eal_init(1, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
    }

    /* Get device information*/
    uint16_t port_id;
    struct rte_eth_dev_info dev_info;
    RTE_ETH_FOREACH_DEV(port_id) {
        rte_eth_dev_info_get(port_id, &dev_info);
        dev_info_list[port_id].port_id = port_id;
        dev_info_list[port_id].num_txq = dev_info.max_tx_queues;
        dev_info_list[port_id].num_rxq = dev_info.max_rx_queues;
        dev_info_list[port_id].mac_addr = get_mac_addr(port_id);
    }
    return 0;
}