/*
 * Author : Juicer
 * BALABALA
 */
#ifndef __tunnel_H__
#define __tunnel_H__

#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/types.h>

//#define Vertual1_To_Juice	
//#define Vertual1_To_Vertual2
//#define Ubuntu_To_Juice
//#define Juice_To_Vertual2	
//#define Juice_To_Ubuntu	
//#define Juice_To_Vertual3
#define Juice_To_Juice2

#define IP_2_u32(A, B, C, D)		(A << 24) | (B << 16) | (C << 8) | D

#if defined Vertual1_To_Juice
#define TERM_2_VEHICLE_SRC_IP		IP_2_u32(192, 168, 11, 25) 	// F411 virtual ubuntu
#define TERM_2_VEHICLE_DST_IP		IP_2_u32(222, 20, 18, 227)	// Juice in dormitory
#elif defined Vertual1_To_Vertual2
#define TERM_2_VEHICLE_SRC_IP		IP_2_u32(192, 168, 11, 24) 	// F411 virtual ubuntu
#define TERM_2_VEHICLE_DST_IP		IP_2_u32(192, 168, 11, 25)	// F411 virtual ubuntu2
#elif defined Ubuntu_To_Juice
#define TERM_2_VEHICLE_SRC_IP		IP_2_u32(192, 168, 11, 2) 	// F411 server
#define TERM_2_VEHICLE_DST_IP		IP_2_u32(192, 168, 11, 5)	// F411 Juice 
#elif defined Juice_To_Vertual2
#define TERM_2_VEHICLE_SRC_IP		IP_2_u32(192, 168, 11, 6)	// F411 Juice 
#define TERM_2_VEHICLE_DST_IP		IP_2_u32(192, 168, 11, 25)	// F411 virtual ubuntu2
#elif defined Juice_To_Ubuntu
#define TERM_2_VEHICLE_SRC_IP		IP_2_u32(192, 168, 11, 5) 	// F411 Juice
#define TERM_2_VEHICLE_DST_IP		IP_2_u32(192, 168, 11, 2)	// F411 server 
#elif defined Juice_To_Vertual3
#define TERM_2_VEHICLE_SRC_IP		IP_2_u32(192, 168, 11, 6)	// F411 Juice 
#define TERM_2_VEHICLE_DST_IP		IP_2_u32(192, 168, 11, 26)	// F411 virtual ubuntu3
#elif defined Juice_To_Juice2
#define TERM_2_VEHICLE_SRC_IP		IP_2_u32(192, 168, 11, 6)	// F411 Juice 
#define TERM_2_VEHICLE_DST_IP		IP_2_u32(192, 168, 11, 5)	// F411 Juice2
#endif

#define UDP_SERV_TEST_PORT 7006
#define TERM_2_VEHICLE_SRC_PORT		7000
#define TERM_2_VEHICLE_DST_PORT		8087




//业务统计
struct traffic_statistics {
	u64	tx_bytes;//发送字节
	u64	tx_pkts;
	u64	tx_drop;

	// rx statistics
	u64	rx_bytes;
	u64	rx_pkts;
	u64	rx_drop;
};

//与本地网卡对应远端网卡信息
struct link_info
{
	u32 dst_ip;//远端IP
	u8 dst_mac[6];//远端MAC
	struct adapter_info *adapter;//本地网络适配器
};

//网卡信息
struct adapter_info
{
	char name[16]; //网卡名称
	struct net_device *dev;//网卡对应的net_device结构
	u8 mac[6];//网卡MAC地址
	u32 ip;//网卡IP地址

	struct traffic_statistics statistics;
};


extern struct link_info _dev_to_link;
extern struct adapter_info _local_adapter;

bool Dev_Judge(struct sk_buff *skb);
bool Link_Init(struct link_info * dev_to_link, struct adapter_info * local_adapter);
struct link_info * Link_Select(void);
int tunneling_tx(struct sk_buff *skb);
void tunnel_udp_init(struct udphdr *udp_data_hdr, struct link_info *dev_to_link, struct sk_buff *skb);
void tunnel_ip_init(struct iphdr *ip_data_hdr, struct link_info *dev_to_link, struct sk_buff *skb);
void tunnel_mac_init(struct ethhdr * eth_data_hdr, struct link_info * dev_to_link, struct sk_buff *skb);

#endif

