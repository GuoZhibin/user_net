/*
 * Author : Juicer
 * BALABALA
 */
#ifndef __debug_H__
#define __debug_H__

#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/types.h>



void IP_int_to_str(uint32_t ip, unsigned int * addr);
void Show_SkBuff_Data(struct sk_buff * skb, bool MAC, bool NET, bool UDP, bool DAT, bool SHINFO);

#endif
