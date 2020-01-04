/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);
    
    /* Add initialization code here! */

} /* -- sr_init -- */


/**
 * This function will send an icmp echo reply when it encounters an icmp echo request
 * @param sr        stores the router instance
 * @param packet    the received packet
 * @param len       length of the received packet
 * @param interface interface of received packet
 * @param type      icmp response type
 * @param code      icmp response code
 * @param dest_if   the destination inerface to send the packet
 */
void send_icmp_packet_0(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface, uint8_t type, uint8_t code, struct sr_if *dest_if)
{
    printf("Inside ICMP 0\n");
    /* Allocate memory to store the icmp packet to be sent */
    uint8_t *send_icmp_packet = (uint8_t*)malloc(len);
    /* Initializing the allocated space */
    memset(send_icmp_packet, 0, sizeof(uint8_t) * len);

    /* Store the address of the ethernet and IP headers of the received packet */
    sr_ethernet_hdr_t *recv_eth_hdr = (sr_ethernet_hdr_t*)packet;
    sr_ip_hdr_t *recv_ip_hdr = (sr_ip_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t));

    /* Initialize the pointer to the location of the IP and ethernet headers of the icmp packet to be sent */
    sr_ip_hdr_t *send_ip_hdr = (sr_ip_hdr_t*)(send_icmp_packet + sizeof(sr_ethernet_hdr_t));
    sr_ethernet_hdr_t *send_eth_hdr = (sr_ethernet_hdr_t*)send_icmp_packet;

    /* Extract the interface to send the packet to */
    struct sr_if *send_if = sr_get_interface(sr, interface);

    /* Extract the destination interface as source interface for the outgoing icmp packet*/
    uint32_t src_ip = send_if->ip;
    if (dest_if)
        src_ip = dest_if->ip;

    /* Initialize the pointer location to the ICMP header */
    sr_icmp_hdr_t *send_icmp_hdr = (sr_icmp_hdr_t*)(send_icmp_packet + sizeof(sr_ip_hdr_t) + sizeof(sr_ethernet_hdr_t));

    /* First fill in the ICMP header */
    memcpy(send_icmp_hdr, (sr_icmp_hdr_t*)(packet + sizeof(sr_ip_hdr_t) + sizeof(sr_ethernet_hdr_t)), len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
    send_icmp_hdr->icmp_code = code;
    send_icmp_hdr->icmp_type = type;
    send_icmp_hdr->icmp_sum = 0;
    send_icmp_hdr->icmp_sum = cksum(send_icmp_hdr, len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));

    /* Then fill in Ethernet Header*/
    memcpy(send_eth_hdr->ether_shost, send_if->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
    memcpy(send_eth_hdr->ether_dhost, recv_eth_hdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
    send_eth_hdr->ether_type = htons(ethertype_ip);

    /* Then fill in the IP header */
    memcpy(send_ip_hdr, recv_ip_hdr, sizeof(sr_ip_hdr_t));
    send_ip_hdr->ip_ttl = INIT_TTL;
    send_ip_hdr->ip_hl = 5;
    send_ip_hdr->ip_id = 0;
    send_ip_hdr->ip_tos = 0;
    send_ip_hdr->ip_v = 4;
    send_ip_hdr->ip_p = ip_protocol_icmp;
    send_ip_hdr->ip_dst = recv_ip_hdr->ip_src;
    send_ip_hdr->ip_len = recv_ip_hdr->ip_len;
    send_ip_hdr->ip_src = src_ip;
    send_ip_hdr->ip_sum = 0;
    send_ip_hdr->ip_sum = cksum(send_ip_hdr, sizeof(sr_ip_hdr_t));
    
    /* Send the ICMP packet and free the pointer */
    sr_send_packet(sr, send_icmp_packet, len, interface);
    free(send_icmp_packet);
}

/**
 * This function will send an icmp response when the icmp message type is 3 or 11
 * @param sr        stores the router instance
 * @param packet    the received packet
 * @param interface interface of received packet
 * @param type      icmp response type
 * @param code      icmp response code
 * @param dest_if   the destination inerface to send the packet
 */
void send_icmp_packet_3_11(struct sr_instance *sr, uint8_t *packet, char *interface, uint8_t type, uint8_t code, struct sr_if *dest_if)
{
    printf("Inside ICMP 3_11\n");
    /* Total header length */
    unsigned int hdr_len = sizeof(sr_icmp_t3_hdr_t) + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t);

    /* Allocate memory to store the icmp packet to be sent */
    uint8_t *send_icmp_packet = (uint8_t*)malloc(hdr_len);
    /* Initializing the allocated space */
    memset(send_icmp_packet, 0, sizeof(uint8_t) * hdr_len);

    /* Store the address of the ethernet and IP headers of the received packet */
    sr_ethernet_hdr_t *recv_eth_hdr = (sr_ethernet_hdr_t*)packet;
    sr_ip_hdr_t *recv_ip_hdr = (sr_ip_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t));

    /* Initialize the pointer to the location of the IP and ethernet headers of the icmp packet to be sent */
    sr_ethernet_hdr_t *send_eth_hdr = (sr_ethernet_hdr_t*)send_icmp_packet;
    sr_ip_hdr_t *send_ip_hdr = (sr_ip_hdr_t*)(send_icmp_packet + sizeof(sr_ethernet_hdr_t));

    /* Extract the interface to send the packet to */
    struct sr_if *send_if = sr_get_interface(sr, interface);

    /* Extract the destination interface as source interface for the outgoing icmp packet*/
    uint32_t src_ip = send_if->ip;
    if (dest_if) {
        printf("Inside 3_11 if\n");
        src_ip = dest_if->ip;
    }

    /* Initialize the pointer location to the ICMP header */
    sr_icmp_t3_hdr_t *send_icmp_t3_hdr = (sr_icmp_t3_hdr_t*)(send_icmp_packet + sizeof(sr_ip_hdr_t) + sizeof(sr_ethernet_hdr_t));

    /* need to add IP header data which is of 8 bytes (already added 20 bytes, need to add 8 bytes as well) */
    memcpy(send_icmp_t3_hdr->data, packet + sizeof(sr_ethernet_hdr_t), 28);

    /* First fill in the ICMP header */
    send_icmp_t3_hdr->icmp_code = code;
    send_icmp_t3_hdr->icmp_type = type;
    send_icmp_t3_hdr->unused = 0;
    send_icmp_t3_hdr->next_mtu = 0;
    send_icmp_t3_hdr->icmp_sum = 0;
    send_icmp_t3_hdr->icmp_sum = cksum(send_icmp_t3_hdr, sizeof(sr_icmp_t3_hdr_t));

    /* Then fill in Ethernet Header*/
    memcpy(send_eth_hdr->ether_shost, send_if->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
    memcpy(send_eth_hdr->ether_dhost, recv_eth_hdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
    send_eth_hdr->ether_type = htons(ethertype_ip);

    /* Then fill in the IP header */
    memcpy(send_ip_hdr, recv_ip_hdr, sizeof(sr_ip_hdr_t));
    send_ip_hdr->ip_ttl = INIT_TTL;
    send_ip_hdr->ip_hl = 5;
    send_ip_hdr->ip_id = 0;
    send_ip_hdr->ip_tos = 0;
    send_ip_hdr->ip_v = 4;
    send_ip_hdr->ip_p = ip_protocol_icmp;
    send_ip_hdr->ip_dst = recv_ip_hdr->ip_src;
    send_ip_hdr->ip_len = htons(hdr_len - sizeof(sr_ethernet_hdr_t));
    send_ip_hdr->ip_src = src_ip;
    send_ip_hdr->ip_sum = 0;
    send_ip_hdr->ip_sum = cksum(send_ip_hdr, sizeof(sr_ip_hdr_t));
    
    /* Send the ICMP packet and free the pointer */
    sr_send_packet(sr, send_icmp_packet, hdr_len, interface);
    free(send_icmp_packet);
}

/**
 * This function handles the working of the IP packet and sends the appropriate response whenever necessary
 * @param sr        stores the router instance
 * @param packet    the received packet
 * @param len       length of the received packet
 * @param interface interface of received packet
 */
void send_ip_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface)
{   
    printf("Inside ip packet\n");
    /* First task is to check the ip header length and throw error if it is less */
    if (len < (sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t))) {
        fprintf(stderr, "Failed to receive IP packet, insufficient length\n");
        return;
    }

    /* Then verify the IP checksum */
    sr_ip_hdr_t *recv_ip_hdr = (sr_ip_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t));
    uint16_t temp = recv_ip_hdr->ip_sum;
    recv_ip_hdr->ip_sum = 0;
    recv_ip_hdr->ip_sum = cksum(recv_ip_hdr, sizeof(sr_ip_hdr_t));
    if (recv_ip_hdr->ip_sum != temp) {
        fprintf(stderr, "IP header checksum failed, does not match calculated checksum\n");
        recv_ip_hdr->ip_sum = temp;
        return;
    }

    struct sr_if *dst_if = fetch_interface_using_ip(sr, recv_ip_hdr->ip_dst);
    if (dst_if) { /* If the received IP packet is destined to one of the router interfaces */
        printf("Inside ip packet and to router interface\n");
        if (recv_ip_hdr->ip_p == ip_protocol_icmp) { /* If the Packet type is ICMP */
            printf("Inside ip packet, router interface and icmp packet\n");
            sr_icmp_hdr_t *recv_icmp_hdr = (sr_icmp_hdr_t*)(packet + sizeof(sr_ip_hdr_t) + sizeof(sr_ethernet_hdr_t));
            /* checking if the length says it is valid icmp packet */
            if (len < (sizeof(sr_icmp_hdr_t) + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t))) {
                fprintf(stderr, "Failed to receive ICMP packet, insufficient length\n");
                return;
            }

            /* Verify ICMP checksum */
            temp = recv_icmp_hdr->icmp_sum;
            recv_icmp_hdr->icmp_sum = 0;
            recv_icmp_hdr->icmp_sum = cksum(recv_icmp_hdr, len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
            if (recv_icmp_hdr->icmp_sum != temp) {
                fprintf(stderr, "ICMP header checksum failed, does not match calculated checksum\n");
                recv_icmp_hdr->icmp_sum = temp;
                return;
            }

            /* If request type is not ping then return */
            if (recv_icmp_hdr->icmp_type != 8) {
                return;
            }

            /* Sending the ICMP echo reply */
            send_icmp_packet_0(sr, packet, len, interface, 0, 0, dst_if);
            fprintf(stdout, "ICMP ping reply sent on interface %s\n", interface);
            return;

            
        } else { /* Packet type is not ICMP */
            send_icmp_packet_3_11(sr, packet, interface, 3, 3, dst_if);
            fprintf(stdout, "ICMP packet received on interface %s has a UDP or TCP payload, hence sending a port unreachable message\n", interface);
            return;
        }
    } else { /* If packet is not to one of the router interfaces */
        printf("Inside ip packet and not to one of router interfaces\n");
        if (recv_ip_hdr->ip_ttl <= 1) {
            fprintf(stderr, "The TTL of the packet has been exhausted, sending time limit exceeded message\n");
            send_icmp_packet_3_11(sr, packet, interface, 11, 0, NULL);
            return;
        }

        /* Identify longest prefix match */
        struct sr_rt * longest_prefix = NULL;
        struct sr_rt * rt_walker = sr->routing_table;
        while(rt_walker)
        {
            printf("Inside longest prefix\n");
            if ((recv_ip_hdr->ip_dst & rt_walker->mask.s_addr) == (rt_walker->dest.s_addr & rt_walker->mask.s_addr)) {
                if (!longest_prefix || (rt_walker->mask.s_addr > longest_prefix->mask.s_addr)) {
                    longest_prefix = rt_walker;
                }
            }
            rt_walker = rt_walker->next;
        }

        /* no match present in the routing table */
        if (!longest_prefix) {
            fprintf(stderr, "The destination IP address was not found in the routing table, sending destionation net unreachable message\n");
            send_icmp_packet_3_11(sr, packet, interface, 3, 0, NULL);
            return;
        }

        /* Decrement TTL */
        recv_ip_hdr->ip_ttl--;
        /* Generate new checksum after change in TTL */
        recv_ip_hdr->ip_sum = 0;
        recv_ip_hdr->ip_sum = cksum(recv_ip_hdr, sizeof(sr_ip_hdr_t));

        /* this is to calculate the next hop mac address from the next hop ip address (longest prefix match) */
        struct sr_arpentry *arp_entry = sr_arpcache_lookup(&(sr->cache), longest_prefix->gw.s_addr);

        if (arp_entry) { /* If ARP cache entry is found */
            /* Fill in the ethernet header */
            sr_ethernet_hdr_t * send_eth_hdr = (sr_ethernet_hdr_t*)(packet);
            memcpy(send_eth_hdr->ether_dhost, arp_entry->mac, sizeof(uint8_t) * ETHER_ADDR_LEN);
            memcpy(send_eth_hdr->ether_shost, sr_get_interface(sr, longest_prefix->interface)->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
            free(arp_entry);
            /* Send the arp packet */
            sr_send_packet(sr, packet, len, sr_get_interface(sr, longest_prefix->interface)->name);
            return;

        } else { /* If ARP cache entry NOT found */
            /* Handling the arp request by adding it to the ARP cache queue*/
            struct sr_arpreq *req = sr_arpcache_queuereq(&sr->cache, longest_prefix->gw.s_addr, packet, len, longest_prefix->interface);
            handle_arpreq(sr, req);
            fprintf(stderr, "ARP cache entry not found, hence queued the ARP request\n");
            return;
        }
    }
}
/**
 * This function handles the working of the arp packet and caches the arp request/reply into the queue
 * @param sr        stores the router instance
 * @param packet    the received packet
 * @param len       length of the received packet
 * @param interface interface of received packet
 */
void send_arp_packet(struct sr_instance *sr, uint8_t *packet, unsigned int len, char *interface)
{
    printf("Inside if it is arp packet\n");
    if (len < (sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t))) {
        fprintf(stderr, "Failed to receive ARP packet, insufficient length\n");
        return;
    }
    /* Extracting the received ARP header */
    sr_arp_hdr_t *recv_arp_hdr = (sr_arp_hdr_t *)(packet  + sizeof(sr_ethernet_hdr_t));
    /* Extracting the received ethernet header */
    sr_ethernet_hdr_t * recv_eth_hdr = (sr_ethernet_hdr_t*)(packet);
    /* Extracting the receiving interface of router */
    struct sr_if * recv_if = sr_get_interface(sr, interface);

    if (arp_op_request == ntohs(recv_arp_hdr->ar_op)) { /* If the received packet is arp request */
        printf("Inside if it is arp request\n");
        if (recv_arp_hdr->ar_tip != recv_if->ip) { /* exit from here since the arp request is not for the router interface */
            return;
        } else { /* Construct an ARP reply packet since the arp request packet target ip is the router interface ip*/
            printf("Build arp reply at router interface\n");

            /* Allocating memory for ARP reply packet */
            uint8_t * arp_reply = (uint8_t*)malloc(len);
            /* Initializing the allocated space */
            memset(arp_reply, 0, len * sizeof(uint8_t));

            /* ARP and ethernet reply headers */
            sr_arp_hdr_t * reply_arp_hdr = (sr_arp_hdr_t*)(arp_reply + sizeof(sr_ethernet_hdr_t));
            sr_ethernet_hdr_t * reply_eth_hdr = (sr_ethernet_hdr_t*)(arp_reply);

            /* First fill in the ethernet header */
            memcpy(reply_eth_hdr->ether_dhost, recv_eth_hdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
            memcpy(reply_eth_hdr->ether_shost, recv_if->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
            reply_eth_hdr->ether_type = htons(ethertype_arp);

            /* Then fill in the ARP header */
            memcpy(reply_arp_hdr, recv_arp_hdr, sizeof(sr_arp_hdr_t));
            reply_arp_hdr->ar_op = htons(arp_op_reply);
            memcpy(reply_arp_hdr->ar_tha, recv_eth_hdr->ether_shost, ETHER_ADDR_LEN);
            memcpy(reply_arp_hdr->ar_sha, recv_if->addr, ETHER_ADDR_LEN);
            reply_arp_hdr->ar_tip = recv_arp_hdr->ar_sip;
            reply_arp_hdr->ar_sip = recv_if->ip;
            sr_send_packet(sr, arp_reply, len, interface);
            free(arp_reply);
        }
    } else if (arp_op_reply == ntohs(recv_arp_hdr->ar_op)) { /* If the received packet is arp reply */
        printf("It is arp reply\n");
        /* This will give us the IP->MAC mapping */
        struct sr_arpreq *arp_req = sr_arpcache_insert(&sr->cache, recv_arp_hdr->ar_sha, recv_arp_hdr->ar_sip);

        /* Iterate through all the waiting packets in the ARP request queue */
        if (arp_req){
            struct sr_packet * pkts_waiting = arp_req->packets;
            while(pkts_waiting) {
                uint8_t *send_packet = pkts_waiting->buf;
                /* First fill in the ethernet header */
                sr_ethernet_hdr_t * send_packet_eth_hdr = (sr_ethernet_hdr_t*)(send_packet);
                memcpy(send_packet_eth_hdr->ether_dhost, recv_arp_hdr->ar_sha, ETHER_ADDR_LEN);
                memcpy(send_packet_eth_hdr->ether_shost, recv_if->addr, ETHER_ADDR_LEN);
                /* Send the packets waiting in the queue */
                sr_send_packet(sr, send_packet, pkts_waiting->len, interface);
                pkts_waiting = pkts_waiting->next;
            }
            /* Frees all memory with respect to this arp request entry */
            sr_arpreq_destroy(&sr->cache, arp_req);
        }
    }
}

/**
 * This function fetches the correct destination interface from the interface list when it is supplied with a 32-bit IP address
 * @param  sr         router instance
 * @param  ip_address 32-bit IP address
 * @return sr_if      destination interface
 */
struct sr_if *fetch_interface_using_ip(struct sr_instance *sr, uint32_t ip_address)
{
    struct sr_if *current_interface = sr->if_list;
    struct sr_if *destination_interface = NULL;
    while (current_interface) {
        if (ip_address == current_interface->ip) { /* left, none */
            destination_interface = current_interface;
            break;
        }
        current_interface = current_interface->next;
    }
    return destination_interface;
}

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
    /* REQUIRES */
    assert(sr);
    assert(packet);
    assert(interface);

    printf("*** -> Received packet of length %d \n",len);

    /* fill in code here */
    uint16_t ethtype = ethertype(packet);

    if (ethtype == ethertype_ip) { /* check if it is IP packet */
        send_ip_packet(sr, packet, len, interface);
    } else if (ethtype == ethertype_arp) { /* check if it is arp packet */
        send_arp_packet(sr, packet, len, interface);
    }
    return;
}/* end sr_ForwardPacket */
