#ifndef DIRVE_H
#define DIRVE_H

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include "mmDebug.h"

#include "llc_encap/llc_encap.h"
#include "tunnel/tunnel.h"

unsigned int postRoutHookDisp(		void *priv, 
									struct sk_buff *skb, 
									const struct nf_hook_state *state);

void term_hook_uldata_process (struct sk_buff *skb);

#endif
