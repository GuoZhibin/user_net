#include "source.h"

unsigned int postRoutHookDisp(		void *priv, 
									struct sk_buff *skb, 
									const struct nf_hook_state *state)
{		
	if(Dev_Judge(skb))
	{
		term_hook_uldata_process(skb);
		return NF_STOLEN;
	}
	else
		return NF_ACCEPT;
}

void term_hook_uldata_process(struct sk_buff *skb)
{
	int FragNum = 0;
	int cnt = 0;

	FragNum = llc_data_encap(skbs, 0, skb, Tx_MTU);

	if(unlikely(FragNum < 0)) return;
	
	for(cnt = 0; cnt < FragNum; cnt++)
	{
		tunneling_tx(skbs[cnt]);
	}
}

