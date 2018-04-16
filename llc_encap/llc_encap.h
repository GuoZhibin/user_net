/*
 * Author : Juicer
 * BALABALA
 */
#ifndef __llc_encap_H__
#define __llc_encap_H__

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/types.h>
#include <net/xfrm.h>

#define ZREOCOPY_LIST
//#define ZEROCOPY_FRAG
//#define NO_ZEROCOPY

#define NEW_HEAD_LEN 68	// mac(14) + ip(20) + udp(8) + llc(8) + reserve(18)

#define Tx_MTU 1300


//数据帧帧头
struct data_hdr
{
	u16 d_or_c:1, //数据还是控制, 1表示控制
		ack_flag:1,// 是否捎带ARQ ACK,如果捎带，则置1，ARQ ACK捎带在数据之后
		frag_flag:2,//分片标志
		llc_id:3,//业务流ID
		res1:1,//保留
		vehicle_id:8; //移动应急车ID
	u16 frag_sn:8,//分片序号
		re_frag_num:4,//再分割包数
		re_frag_sn:4;//再分割序号
	u32 pkt_sn:12,//数据包序号
		len:14,//数据包长度，不包括包头struct data_hdr
		res3:6; //保留
};

//控制帧类型
typedef enum _ctl_frame_type
{
	LLC_RESET,//LLC复位
	ARQ_ACK//ARQ确认
}ctl_frame_type;


//控制帧帧头
struct ctr_hdr
{
	u16 d_or_c:1,  //数据还是控制, 1表示控制
		ctl_frame_t:4,//控制帧类型
		res1:3,//保留
		vehicle_id:8;//移动应急车ID
	u16 len:8,//控制字段长度，不包括控制头部4字节长度
		res2:8; //保留
};


//业务流ID
typedef enum _llc_traffic_id
{
	DATA_TM0   = 0x00,		//透明传输
	DATA_TM1,		//透明传输
	DATA_AM0,		//AM业务，业务对时间不太敏感，对业务质量要求高
	DATA_AM1, 		//AM业务，业务对时间不太敏感，对业务质量要求高
	DATA_AM2,		//AM业务，业务对时间敏感度较高，对业务质量要求高
	DATA_AM3,		//AM业务，业务对时间敏感度较高，对业务质量要求高
	DATA_UM0,		//UM业务，时间敏感度高，业务质量要求一般
	DATA_UM1		//UM业务，时间敏感度高，业务质量要求一般
}llc_traffic_id;



/* RTP data header */
struct rtphdr
{
    unsigned int cc:4;        /* CSRC count */
    unsigned int x:1;         /* header extension flag */
    unsigned int p:1;         /* padding flag */
    unsigned int version:2;   /* protocol version */
	unsigned int pt:7;        /* payload type */
    unsigned int m:1;         /* marker bit */
	unsigned int seq:16;      /* sequence number */
    unsigned long ts;         /* timestamp */
    unsigned long ssrc;       /* synchronization source */
    unsigned long csrc[1];    /* optional CSRC list */
};

struct rtp_buffer
{
	bool valid;
	unsigned long jiffies;
	__be32 saddr;
	__be32 daddr;
	__be16 sport;
	__be16 dport;
	unsigned int seq;
	unsigned int pt;
	unsigned long ssrc;
};

#define MAX_FRAG_NUM	512
extern struct sk_buff * skbs[MAX_FRAG_NUM];
#define MAX_RTP_NUM		16
extern struct rtp_buffer rtps[MAX_RTP_NUM];

int llc_data_encap(struct sk_buff ** skbs, int FragNum, struct sk_buff * skb, unsigned int MTU);
int llc_ctrl_encap(struct sk_buff *skb, ctl_frame_type type, unsigned int llcID, unsigned int MTU);


void skb_header_copy(struct sk_buff *new, const struct sk_buff *old);
void SKBS_FREE(struct sk_buff ** skbs, struct sk_buff *skb, int FragNum);
int TOO_MUCH_FRAGMENTS(struct sk_buff ** skbs, struct sk_buff *skb, int FragNum);
int SKB_ALLOC_FAILED(struct sk_buff ** skbs, struct sk_buff *skb, int FragNum);
llc_traffic_id TrafficID(struct sk_buff *skb);
void LLC_Data_Init(struct data_hdr *LLC_data_hdr, struct sk_buff *skb);
struct sk_buff * skb_add_head_list(unsigned int len, struct sk_buff * skb_para);


#endif

