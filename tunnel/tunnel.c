#include "tunnel.h"

struct link_info _dev_to_link;
struct adapter_info _local_adapter;

bool Dev_Judge(struct sk_buff *skb)
{
	struct udphdr * udphdr_t = (struct udphdr *)(skb->head + skb->transport_header);
	if(ntohs(udphdr_t->dest) == UDP_SERV_TEST_PORT)
		return true;
	else
		return false;
} 

bool Link_Init(struct link_info * dev_to_link, struct adapter_info * local_adapter)	
{

#if defined Vertual1_To_Juice || defined Vertual1_To_Vertual2
	unsigned char name[] = "ens33";
#elif defined Ubuntu_To_Juice
	unsigned char name[] = "eno2";
#elif defined Juice_To_Vertual2 || defined Juice_To_Ubuntu || defined Juice_To_Vertual3 \
								|| defined Juice_To_Juice2
	unsigned char name[] = "enp4s2";
#else
	unsigned char name[] = "ens33";
#endif	

#if defined Vertual1_To_Juice
	unsigned char TERM_2_VEHICLE_SRC_MAC[6] = {0x00, 0x0c, 0x29, 0x17, 0xe9, 0xc6};	// F411 virtual ubuntu
	unsigned char TERM_2_VEHICLE_DST_MAC[6] = {0x14, 0x18, 0x77, 0x48, 0x8d, 0x1c};	// Juice in dormitory
#elif defined Vertual1_To_Vertual2
	unsigned char TERM_2_VEHICLE_SRC_MAC[6] = {0x00, 0x0c, 0x29, 0x4e, 0x5a, 0xbe}; // F411 virtual ubuntu
	unsigned char TERM_2_VEHICLE_DST_MAC[6] = {0x00, 0x0c, 0x29, 0x6e, 0xeb, 0x38}; // F411 virtual ubuntu2
#elif defined Ubuntu_To_Juice
	unsigned char TERM_2_VEHICLE_SRC_MAC[6] = {0xb8, 0xca, 0x3a, 0xed, 0x55, 0x2b}; // F411 server
	unsigned char TERM_2_VEHICLE_DST_MAC[6] = {0x00, 0x25, 0x11, 0x5d, 0x03, 0x9b}; // F411 Juice
#elif defined Juice_To_Vertual2
	unsigned char TERM_2_VEHICLE_SRC_MAC[6] = {0x00, 0x25, 0x11, 0x5d, 0x03, 0x9b}; // F411 Juice
	unsigned char TERM_2_VEHICLE_DST_MAC[6] = {0x00, 0x0c, 0x29, 0x6e, 0xeb, 0x38}; // F411 virtual ubuntu2
#elif defined Juice_To_Ubuntu
	unsigned char TERM_2_VEHICLE_SRC_MAC[6] = {0x00, 0x25, 0x11, 0x5d, 0x03, 0x9b}; // F411 Juice
	unsigned char TERM_2_VEHICLE_DST_MAC[6] = {0xb8, 0xca, 0x3a, 0xed, 0x55, 0x2b}; // F411 server
#elif defined Juice_To_Vertual3
	unsigned char TERM_2_VEHICLE_SRC_MAC[6] = {0x00, 0x25, 0x11, 0x5d, 0x03, 0x9b}; // F411 Ubuntu13
	unsigned char TERM_2_VEHICLE_DST_MAC[6] = {0x00, 0x0c, 0x29, 0xb6, 0x23, 0xfb}; // F411 Vertual3
#elif defined Juice_To_Juice2
	unsigned char TERM_2_VEHICLE_SRC_MAC[6] = {0x00, 0x25, 0x11, 0x5d, 0x03, 0x9b}; // F411 Juice
	unsigned char TERM_2_VEHICLE_DST_MAC[6] = {0x10, 0x78, 0xd2, 0xa9, 0x42, 0x2b}; // F411 Juice2
#endif

	memcpy(local_adapter->name, name, sizeof(name));
	local_adapter->dev = NULL;	
	memcpy(local_adapter->mac , TERM_2_VEHICLE_SRC_MAC, 6);
	local_adapter->ip = TERM_2_VEHICLE_SRC_IP;

	dev_to_link->dst_ip = TERM_2_VEHICLE_DST_IP;
	memcpy(dev_to_link->dst_mac, TERM_2_VEHICLE_DST_MAC, 6);
	dev_to_link->adapter = local_adapter;

	return true;
}

struct link_info * Link_Select(void)
{
	return &_dev_to_link;
}

int tunneling_tx(struct sk_buff *skb)
{
	struct link_info * dev_to_link = Link_Select();
	struct udphdr * UDP_data_hdr;
	struct iphdr * IP_data_hdr;	
	struct ethhdr * EHT_data_hdr;
	//***********************Encapsulate UDP head**********************************
	UDP_data_hdr = (struct udphdr *)skb_push(skb, sizeof(struct udphdr));				// Make UDP head area
	skb_reset_transport_header(skb);						// Update skb->transport_header
	tunnel_udp_init(UDP_data_hdr, dev_to_link, skb);	// Init UDP head

	//***********************Encapsulate IP head**********************************
	IP_data_hdr = (struct iphdr *)skb_push(skb, sizeof(struct iphdr));				// Make IP head area
	skb_reset_network_header(skb);							// Update skb->network_header
	tunnel_ip_init(IP_data_hdr, dev_to_link, skb);	// Init IP head

	//***********************Encapsulate ETH head**********************************
	EHT_data_hdr = (struct ethhdr *)skb_push(skb, sizeof(struct ethhdr));				// Make ETH head area
	skb_reset_mac_header(skb);										// Update skb->mac_header
	tunnel_mac_init(EHT_data_hdr, dev_to_link, skb);	// Init ETH head

	//-----------------------Encapsulation Done---------------------------------------------
	
	if(unlikely(!skb->dev)) 
	{
		printk(KERN_ERR"ERROR : dev is null.\n");
		kfree_skb(skb);
		return -1;
	}
//	dev_hold(skb->dev);
    return dev_queue_xmit(skb);
}


void tunnel_udp_init(struct udphdr *udp_data_hdr, struct link_info *dev_to_link, struct sk_buff *skb)
{
	unsigned int datalen = skb->len;

	udp_data_hdr->source = htons(TERM_2_VEHICLE_SRC_PORT);	
	udp_data_hdr->dest = htons(TERM_2_VEHICLE_DST_PORT);
 	udp_data_hdr->len = htons(datalen);
	udp_data_hdr->check = 0x00;	
/* UDP checknum will not be used when set 0 */
//	skb->csum = csum_partial(udp_data_hdr, datalen, skb->csum);
// 	udp_data_hdr->check = csum_tcpudp_magic(htonl(dev_to_link->adapter->ip), 
//											htonl(dev_to_link->dst_ip), datalen, IPPROTO_UDP, skb->csum);
}

void tunnel_ip_init(struct iphdr *ip_data_hdr, struct link_info *dev_to_link, struct sk_buff *skb)
{
	static u16 ip_id = 0;

	ip_data_hdr->version = 4;										// IPv4
	ip_data_hdr->ihl = sizeof(struct iphdr) >> 2;					// Head size
	ip_data_hdr->tos = 0;											// Normal server
	ip_data_hdr->tot_len = htons((u64)(skb->len));					// Total length
	ip_data_hdr->id = htons(ip_id++);								// IP id
	ip_data_hdr->frag_off = htons(0x0000);							// Frag & Offset	
	ip_data_hdr->ttl = IPDEFTTL;									// TTL
	ip_data_hdr->protocol = IPPROTO_UDP;							// Protocal : UDP
	ip_data_hdr->check = 0;											// IP check
	ip_data_hdr->saddr = htonl(dev_to_link->adapter->ip);			// Src IP
	ip_data_hdr->daddr = htonl(dev_to_link->dst_ip);				// Dst IP

	ip_data_hdr->check = ip_fast_csum((unsigned char *)ip_data_hdr, ip_data_hdr->ihl); 	// Compute checksum

	skb->ip_summed = CHECKSUM_NONE;
	skb->protocol = htons(ETH_P_IP);

	if(ip_id >= 65536) ip_id = 0;

}

void tunnel_mac_init(struct ethhdr * eth_data_hdr, struct link_info * dev_to_link, struct sk_buff *skb)
{
	memcpy(eth_data_hdr->h_source, dev_to_link->adapter->mac, ETH_ALEN); 
	memcpy(eth_data_hdr->h_dest, dev_to_link->dst_mac, ETH_ALEN); 
	eth_data_hdr->h_proto = htons(ETH_P_IP);
	skb->dev = dev_get_by_name(&init_net, dev_to_link->adapter->name);
	if(unlikely(!skb->dev))
		printk(KERN_ERR"ERROR : skb->dev is NULL\n");
	/* skb->dev was hold in dev_get_by_name() */
	dev_put(skb->dev);
}


