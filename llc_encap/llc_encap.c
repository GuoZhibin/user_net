#include "llc_encap.h"

struct sk_buff * skbs[MAX_FRAG_NUM];
struct rtp_buffer rtps[MAX_RTP_NUM];


#if defined ZREOCOPY_LIST
int llc_data_encap(struct sk_buff ** skbs, int FragNum, struct sk_buff * skb, unsigned int MTU)
{
	struct skb_shared_info skbshinfo = *skb_shinfo(skb);
	struct skb_shared_info * shinfo = NULL;
	unsigned int LinearFragNum = 0;
	struct sk_buff * SkbClone = NULL;
	unsigned char * skb_ptr = NULL;
	struct data_hdr * LLC_data_hdr = NULL;	
	struct sk_buff * new_skb = NULL;

	unsigned int slicecnt = 0, slicelen = 0, fragcnt = 0, slice_num = 0;							

	unsigned int max_data_len = MTU - NEW_HEAD_LEN;
	netdev_features_t features = netif_skb_features(skb);

	if(unlikely(max_data_len <= 0)) return -1;

	/* Judge if network device support SG mode */
	if(!(features & NETIF_F_FRAGLIST))
	{
		printk("NETIF_F_FRAGLIST is unsupported.\n");
		SKBS_FREE(skbs, skb, FragNum);
		return -1;
	}

	if(skbshinfo.gso_size)
	{
		printk("GSO is unsupported.\n");
		SKBS_FREE(skbs, skb, FragNum);
		return -1;
	}

	/* This is unnecessary when running in a router */
	if(skb->ip_summed == CHECKSUM_PARTIAL)
	{
		struct iphdr *ip_hdr = (struct iphdr *)skb_network_header(skb);
		if(ip_hdr->protocol == IPPROTO_UDP)
		{
			struct udphdr *udp_hdr = (struct udphdr *)skb_transport_header(skb);
			udp_hdr->check = 0x00;
			skb->csum = 0;
			skb->csum = csum_partial(udp_hdr, ntohs(udp_hdr->len), 0);
			udp_hdr->check = csum_tcpudp_magic(ip_hdr->saddr, ip_hdr->daddr, 
											ntohs(udp_hdr->len), IPPROTO_UDP, skb->csum);
		}
		else if(ip_hdr->protocol == IPPROTO_TCP)
		{
			struct tcphdr *tcp_hdr = (struct tcphdr *)skb_transport_header(skb);
			tcp_hdr->check = 0x00;
			skb->csum = csum_partial(tcp_hdr, tcp_hdr->doff << 2, 0);
			tcp_hdr->check = csum_tcpudp_magic(ip_hdr->saddr, ip_hdr->daddr, 
											skb->len - sizeof(struct iphdr), IPPROTO_TCP, skb->csum);
		}
		skb->ip_summed = CHECKSUM_NONE;
	}


	// Small skb with no frag_list, don't need to clone, and it must be the last one.
	if(skb->len <= max_data_len) //  && !skb_is_nonlinear(skb)
	{	
		new_skb = skb_add_head_list(NEW_HEAD_LEN, skb);
		if (unlikely(!new_skb))	
			return SKB_ALLOC_FAILED(skbs, skb, FragNum);
		skbs[FragNum++] = new_skb;
		if(unlikely(FragNum > MAX_FRAG_NUM))	
			return TOO_MUCH_FRAGMENTS(skbs, skb, FragNum);

		LLC_data_hdr = (struct data_hdr *)skb_push(new_skb, sizeof(struct data_hdr)); 		// Make LLC head area.
		LLC_Data_Init(LLC_data_hdr, skb); 	// Init LLC head
		LLC_data_hdr->frag_sn = FragNum - 1;

		if(FragNum > 1)	// We are now in frag_list
			LLC_data_hdr->frag_flag = 1;		// Last frag
		return FragNum;
	}

	// Linear data split
	shinfo = skb_shinfo(skb);
	shinfo->nr_frags = 0;
	shinfo->gso_size = 0;
	shinfo->gso_segs = 0;
	shinfo->gso_type = 0;
	shinfo->frag_list = NULL;

	LinearFragNum = skb_headlen(skb) / max_data_len;
	skb_ptr = skb->data;
	for(slicecnt = 0; slicecnt <= LinearFragNum; slicecnt++)
	{
		if(slicecnt == LinearFragNum) 
			slicelen = skb_headlen(skb) - LinearFragNum * max_data_len;
		else 
			slicelen = max_data_len;

		SkbClone = skb_clone(skb, GFP_ATOMIC);
		if(!SkbClone)
		{
			printk("skb clone failed\n");
			SKBS_FREE(skbs, skb, FragNum);
			return -1;
		}
		SkbClone->data = skb_ptr;
		skb_set_tail_pointer(SkbClone, slicelen);
		SkbClone->len = slicelen;
		SkbClone->data_len = 0;
		SkbClone->truesize += slicelen;
		skb_ptr += slicelen;

		new_skb = skb_add_head_list(NEW_HEAD_LEN, SkbClone);
		if (unlikely(!new_skb))	
			return SKB_ALLOC_FAILED(skbs, skb, FragNum);

		skbs[FragNum++] = new_skb;
		if(unlikely(FragNum > MAX_FRAG_NUM))	
			return TOO_MUCH_FRAGMENTS(skbs, skb, FragNum);
		
		LLC_data_hdr = (struct data_hdr *)skb_push(new_skb, sizeof(struct data_hdr)); 		// Make LLC head area.
		LLC_Data_Init(LLC_data_hdr, skb); 	// Init LLC head
		LLC_data_hdr->frag_sn = FragNum - 1;

		// First frag : FragNum == 1 && Have frags in linear or nonlinear data
		if((FragNum == 1) && (LinearFragNum || skbshinfo.nr_frags || skbshinfo.frag_list)) 
			LLC_data_hdr->frag_flag = 2;
		// Last frag : slicecnt == LinearFragNum && Have frags in linear data && no frags in nonlinear data
		else if((slicecnt == LinearFragNum) && (LinearFragNum) && (skb->next == NULL)
								 && (skbshinfo.nr_frags == 0) && (skbshinfo.frag_list == NULL)) 
		{
			LLC_data_hdr->frag_flag = 1; 
			kfree_skb(skb); 
			return FragNum;
		}
		// Middle frag
		else LLC_data_hdr->frag_flag = 0;
	}

	// Nonlinear data split
	// page frags split
	if(unlikely(skbshinfo.nr_frags))
	{
		skb_frag_t *skb_frag = NULL;
		unsigned int offset = 0;
		
		for(fragcnt = 0; fragcnt < skbshinfo.nr_frags; fragcnt++)
		{
			skb_frag = &(skbshinfo.frags[fragcnt]);
			slice_num =  skb_frag_size(skb_frag) / max_data_len;
			offset = skb_frag->page_offset;
			
			for(slicecnt = 0; slicecnt <= slice_num; slicecnt++)
			{
				if(slicecnt == slice_num) 
					slicelen = skb_frag_size(skb_frag) - slice_num * max_data_len;
				else 
					slicelen = max_data_len;

				new_skb = alloc_skb(NEW_HEAD_LEN, GFP_ATOMIC);
				if (unlikely(!new_skb))	
					return SKB_ALLOC_FAILED(skbs, skb, FragNum);
				skb_reserve(new_skb, NEW_HEAD_LEN);
				skb_header_copy(new_skb, skb);
				
				skbs[FragNum++] = new_skb;
				if(unlikely(FragNum > MAX_FRAG_NUM))	
					return TOO_MUCH_FRAGMENTS(skbs, skb, FragNum);

				skb_fill_page_desc(new_skb, 0, skb_frag_page(skb_frag), offset, slicelen);
				skb_frag_ref(new_skb, 0);
				offset += slicelen;

				new_skb->len = slicelen;
				new_skb->data_len = slicelen;
				new_skb->truesize += slicelen;
				
				LLC_data_hdr = (struct data_hdr *)skb_push(new_skb, sizeof(struct data_hdr)); 		// Make LLC head area.
				LLC_Data_Init(LLC_data_hdr, skb); 	// Init LLC head
				LLC_data_hdr->frag_sn = FragNum - 1;

				// Last frag : This is last frag in last page && no frag_list
				if((fragcnt == (skbshinfo.nr_frags - 1)) && (slicecnt == slice_num) 
															&& (skbshinfo.frag_list == NULL))
				{
					LLC_data_hdr->frag_flag = 1; 
					kfree_skb(skb); 
					return FragNum;
				}
				// Middle Frag : Certainly not the first frag.
				else 
					LLC_data_hdr->frag_flag = 0;
			}
		}
	}

	// Nonlinear data split
	// frag_list split
	if(unlikely(skbshinfo.frag_list))
	{
		FragNum = llc_data_encap(skbs, FragNum, skbshinfo.frag_list, MTU);
	}
	if(unlikely(skb->next))
	{
		FragNum = llc_data_encap(skbs, FragNum, skb->next, MTU);
	}

	// skb is useless now, free it here.
	kfree_skb(skb); 
	
	return FragNum;
}
#elif defined ZEROCOPY_FRAG
int llc_encap(struct sk_buff ** skbs, int FragNum, struct sk_buff * skb, unsigned int MTU)
{
	struct skb_shared_info *skbshinfo = NULL;
	struct data_hdr * LLC_data_hdr = NULL;	
	unsigned int slicecnt = 0, slicelen = 0, fragcnt = 0, slice_num = 0;	
	skb_frag_t *skb_frag = NULL;
	struct sk_buff * new_skb = NULL;
	struct page * page = NULL;
	unsigned int offset = 0, page_size = 0;
	unsigned int max_data_len = MTU - NEW_HEAD_LEN;
	int len_to_split = 0;
	netdev_features_t features = netif_skb_features(skb);

	if(unlikely(max_data_len <= 0)) return -1;

	/* Judge if network device support SG mode */
	if(!(features & NETIF_F_SG))
	{
		printk("NETIF_F_SG is unsupported.\n");
		kfree_skb(skb);
		return 0;
	}

	/* This is unnecessary when running in a router */
 	if(unlikely(skb->ip_summed == CHECKSUM_PARTIAL))
	{
		struct iphdr *iph = (struct iphdr *)skb_network_header(skb);
		if(iph->protocol == IPPROTO_UDP)
		{
			struct udphdr * udph = (struct udphdr *)skb_transport_header(skb);
			udph->check = 0;
		}
		else if(iph->protocol == IPPROTO_TCP)
		{
			struct tcphdr * th = (struct tcphdr *)skb_transport_header(skb);
			th->check = 0;
			th->check = csum_tcpudp_magic(iph->saddr, iph->daddr, skb->len - sizeof(struct iphdr), 
													IPPROTO_TCP, csum_partial(th, th->doff << 2, 0));
		}
		skb->ip_summed = CHECKSUM_NONE;
 	}

	printk("skb->head_frag = %d\n", skb->head_frag);
	if(!skb->head_frag)
	{
		printk("Page frag unsupported!\n");
		kfree_skb(skb);
		return 0;
	}

	/* Linear data split */
	len_to_split = skb_headlen(skb);
	page = virt_to_head_page(skb->head);
	offset = skb->data - (unsigned char *)page_address(page);
	while(len_to_split > 0)
	{
		new_skb = alloc_skb(NEW_HEAD_LEN, GFP_ATOMIC);
		if (unlikely(!new_skb))	
			return SKB_ALLOC_FAILED(skbs, skb, FragNum);
		skb_reserve(new_skb, NEW_HEAD_LEN);
		skb_header_copy(new_skb, skb);

		skbs[FragNum++] = new_skb;
		if(unlikely(FragNum > MAX_FRAG_NUM))	
			return TOO_MUCH_FRAGMENTS(skbs, skb, FragNum);
		/* Change skb's linear data to new_skb's page */
		page_size = len_to_split > max_data_len ? max_data_len : len_to_split;
		len_to_split -= page_size;		
		skb_fill_page_desc(new_skb, 0, page, offset, page_size);
//		skb_frag_ref(new_skb, 0);
		get_page(page);
		offset += page_size;

		/* Update new_skb data */
		new_skb->len += page_size;
		new_skb->data_len += page_size;
		new_skb->truesize += page_size;

		/* Fill LLC data */
		LLC_data_hdr = (struct data_hdr *)skb_push(new_skb, sizeof(struct data_hdr)); 		
		LLC_Data_Init(LLC_data_hdr, skb);
		LLC_data_hdr->frag_sn = FragNum - 1;
		
		/* frag_flag set, default is No_Fragment */
		if(len_to_split || skb_is_nonlinear(skb))
		{
			if(FragNum > 1)		LLC_data_hdr->frag_flag = 0;	// Middle
			else				LLC_data_hdr->frag_flag = 2;	// First
		}
		else if(FragNum > 1)	// Last 	
		{
			LLC_data_hdr->frag_flag = 1;
			kfree_skb(skb); 	
			return FragNum;
		}
		else	// First && Only one
		{
			kfree_skb(skb); 
			return FragNum;	
		}
	}

	// Nonlinear data split
	// page frags split
	skbshinfo = (struct skb_shared_info *)skb_shinfo(skb);
	if(unlikely(skbshinfo->nr_frags))
	{
		for(fragcnt = 0; fragcnt < skbshinfo->nr_frags; fragcnt++)
		{
			skb_frag = &skbshinfo->frags[fragcnt];
			slice_num = skb_frag_size(skb_frag) / max_data_len;
			offset = skb_frag->page_offset;

			for(slicecnt = 0; slicecnt <= slice_num; slicecnt++)
			{
				if(slicecnt == slice_num) 
					slicelen = skb_frag->size - slice_num * max_data_len;
				else 
					slicelen = max_data_len;

				new_skb = alloc_skb(NEW_HEAD_LEN, GFP_ATOMIC);
				if (unlikely(!new_skb))	
					return SKB_ALLOC_FAILED(skbs, skb, FragNum);
				skb_reserve(new_skb, NEW_HEAD_LEN);
				skb_header_copy(new_skb, skb);				

				skbs[FragNum++] = new_skb;
				if(unlikely(FragNum > MAX_FRAG_NUM))	
					return TOO_MUCH_FRAGMENTS(skbs, skb, FragNum);

				skb_fill_page_desc(new_skb, 0, skb_frag_page(skb_frag), offset, slicelen);
				skb_frag_ref(new_skb, 0);
				offset += slicelen;

				/* Update new_skb data */
				new_skb->len += slicelen;
				new_skb->data_len += slicelen;
				new_skb->truesize += slicelen;
				
				/* Fill LLC data */
				LLC_data_hdr = (struct data_hdr *)skb_push(new_skb, sizeof(struct data_hdr)); 
				LLC_Data_Init(LLC_data_hdr, skb);
				LLC_data_hdr->frag_sn = FragNum - 1;	

				// Last frag : This is last frag in last page && no frag_list
				if((fragcnt == (skbshinfo->nr_frags - 1)) 
											&& (slicecnt == slice_num) 
											&& (skbshinfo->frag_list == NULL))
				{
					LLC_data_hdr->frag_flag = 1; 
					kfree_skb(skb); 
					return FragNum;
				}
				// Middle Frag : Certainly not the first frag.
				else 
					LLC_data_hdr->frag_flag = 0;
			}
		}
	}

	// Nonlinear data split
	// frag_list split
	if(unlikely(skbshinfo->frag_list))
	{
		FragNum = llc_data_encap(skbs, FragNum, skbshinfo->frag_list, MTU);
		skbshinfo->frag_list = NULL;
	}
	if(unlikely(skb->next))
	{
		FragNum = llc_data_encap(skbs, FragNum, skb->next, MTU);
		skb->next = NULL;
	}

	// skb is useless now, free it here.
	kfree_skb(skb); 
	
	return FragNum;
}
#elif defined NO_ZEROCOPY
int llc_encap(struct sk_buff ** skbs, int FragNum, struct sk_buff * skb, unsigned int MTU)
{
	struct skb_shared_info skbshinfo = *skb_shinfo(skb);
	struct skb_shared_info * shinfo = NULL;
	unsigned int LinearFragNum = 0;
	unsigned char * skb_ptr = NULL;
	struct data_hdr * LLC_data_hdr = NULL;	
	struct sk_buff * new_skb = NULL;

	unsigned int slicecnt = 0, slicelen = 0, fragcnt = 0, slice_num = 0;							

	unsigned int max_data_len = MTU - NEW_HEAD_LEN;

	if(unlikely(max_data_len <= 0)) return -1;

	if(skbshinfo.gso_size)
	{
		printk("GSO is unsupported.\n");
		SKBS_FREE(skbs, skb, FragNum);
		return -1;
	}

	/* This is unnecessary when running in a router */
	if(skb->ip_summed == CHECKSUM_PARTIAL)
	{
		struct iphdr *ip_hdr = (struct iphdr *)skb_network_header(skb);
		if(ip_hdr->protocol == IPPROTO_UDP)
		{
			struct udphdr *udp_hdr = (struct udphdr *)skb_transport_header(skb);
			udp_hdr->check = 0x00;
			skb->csum = 0;
			skb->csum = csum_partial(udp_hdr, ntohs(udp_hdr->len), 0);
			udp_hdr->check = csum_tcpudp_magic(ip_hdr->saddr, ip_hdr->daddr, 
											ntohs(udp_hdr->len), IPPROTO_UDP, skb->csum);
		}
		else if(ip_hdr->protocol == IPPROTO_TCP)
		{
			struct tcphdr *tcp_hdr = (struct tcphdr *)skb_transport_header(skb);
			tcp_hdr->check = 0x00;
			skb->csum = csum_partial(tcp_hdr, tcp_hdr->doff << 2, 0);
			tcp_hdr->check = csum_tcpudp_magic(ip_hdr->saddr, ip_hdr->daddr, 
											skb->len - sizeof(struct iphdr), IPPROTO_TCP, skb->csum);
		}
		skb->ip_summed = CHECKSUM_NONE;
	}

	// Linear data split
	shinfo = skb_shinfo(skb);
	shinfo->nr_frags = 0;
	shinfo->gso_size = 0;
	shinfo->gso_segs = 0;
	shinfo->gso_type = 0;
	shinfo->frag_list = NULL;

	LinearFragNum = skb_headlen(skb) / max_data_len;
	skb_ptr = skb->data;
	for(slicecnt = 0; slicecnt <= LinearFragNum; slicecnt++)
	{
		if(slicecnt == LinearFragNum)
			slicelen = skb_headlen(skb) - LinearFragNum * max_data_len;
		else 
			slicelen = max_data_len;

		new_skb = alloc_skb(NEW_HEAD_LEN + slicelen, GFP_ATOMIC);
		if (unlikely(!new_skb))	
			return SKB_ALLOC_FAILED(skbs, skb, FragNum);
		skb_reserve(new_skb, NEW_HEAD_LEN);
		skb_header_copy(new_skb, skb);
		
		memcpy(skb_put(new_skb, slicelen), skb_ptr, slicelen);
		skb_ptr += slicelen;

		skbs[FragNum++] = new_skb;
		if(unlikely(FragNum > MAX_FRAG_NUM))	
			return TOO_MUCH_FRAGMENTS(skbs, skb, FragNum);
		
		LLC_data_hdr = (struct data_hdr *)skb_push(new_skb, sizeof(struct data_hdr)); 		// Make LLC head area.
		LLC_Data_Init(LLC_data_hdr, skb); 	// Init LLC head
		LLC_data_hdr->frag_sn = FragNum - 1;

		// First frag : FragNum == 1 && Have frags in linear or nonlinear data
		if((FragNum == 1) && (LinearFragNum || skbshinfo.nr_frags || skbshinfo.frag_list)) 
			LLC_data_hdr->frag_flag = 2;
		// Last frag : slicecnt == LinearFragNum && Have frags in linear data && no frags in nonlinear data
		else if((slicecnt == LinearFragNum) && (LinearFragNum) && (skb->next == NULL)
								 && (skbshinfo.nr_frags == 0) && (skbshinfo.frag_list == NULL)) 
		{
			LLC_data_hdr->frag_flag = 1; 
			kfree_skb(skb); 
			return FragNum;
		}
		// Middle frag
		else LLC_data_hdr->frag_flag = 0;
	}

	// Nonlinear data split
	// page frags split
	if(unlikely(skbshinfo.nr_frags))
	{
		skb_frag_t *skb_frag = NULL;
		unsigned int offset = 0;
		
		for(fragcnt = 0; fragcnt < skbshinfo.nr_frags; fragcnt++)
		{
			skb_frag = &(skbshinfo.frags[fragcnt]);
			slice_num =  skb_frag_size(skb_frag) / max_data_len;
			offset = skb_frag->page_offset;
			
			for(slicecnt = 0; slicecnt <= slice_num; slicecnt++)
			{
				if(slicecnt == slice_num) 
					slicelen = skb_frag_size(skb_frag) - slice_num * max_data_len;
				else 
					slicelen = max_data_len;

				new_skb = alloc_skb(NEW_HEAD_LEN, GFP_ATOMIC);
				if (unlikely(!new_skb))	
					return SKB_ALLOC_FAILED(skbs, skb, FragNum);
				skb_reserve(new_skb, NEW_HEAD_LEN);
				skb_header_copy(new_skb, skb);
				
				skbs[FragNum++] = new_skb;
				if(unlikely(FragNum > MAX_FRAG_NUM))	
					return TOO_MUCH_FRAGMENTS(skbs, skb, FragNum);

				skb_fill_page_desc(new_skb, 0, skb_frag_page(skb_frag), offset, slicelen);
				skb_frag_ref(new_skb, 0);
				offset += slicelen;

				new_skb->len = slicelen;
				new_skb->data_len = slicelen;
				new_skb->truesize += slicelen;
				
				LLC_data_hdr = (struct data_hdr *)skb_push(new_skb, sizeof(struct data_hdr)); 		// Make LLC head area.
				LLC_Data_Init(LLC_data_hdr, skb); 	// Init LLC head
				LLC_data_hdr->frag_sn = FragNum - 1;

				// Last frag : This is last frag in last page && no frag_list
				if((fragcnt == (skbshinfo.nr_frags - 1)) && (slicecnt == slice_num) 
															&& (skbshinfo.frag_list == NULL))
				{
					LLC_data_hdr->frag_flag = 1; 
					kfree_skb(skb); 
					return FragNum;
				}
				// Middle Frag : Certainly not the first frag.
				else 
					LLC_data_hdr->frag_flag = 0;
			}
		}
	}

	// Nonlinear data split
	// frag_list split
	if(unlikely(skbshinfo.frag_list))
	{
		FragNum = llc_data_encap(skbs, FragNum, skbshinfo.frag_list, MTU);
	}
	if(unlikely(skb->next))
	{
		FragNum = llc_data_encap(skbs, FragNum, skb->next, MTU);
	}

	// skb is useless now, free it here.
	kfree_skb(skb); 
	
	return FragNum;
}
#endif

int llc_ctrl_encap(struct sk_buff *skb, ctl_frame_type type, unsigned int llcID, unsigned int MTU)
{		
	struct ctr_hdr * ctrh = NULL;
	unsigned int len = skb->len;
	
	if(unlikely(skb_headroom(skb) < NEW_HEAD_LEN))
	{
		printk("ERROR:skb_headroom(skb) < NEW_HEAD_LEN \nfunc:llc_ctrl_encap\n");
		kfree_skb(skb);
		return -1;
	}

	if(unlikely(skb->len + NEW_HEAD_LEN > MTU))
	{
		printk("ERROR:skb->len + NEW_HEAD_LEN > MTU \nfunc:llc_ctrl_encap\n");
		kfree_skb(skb);
		return -1;
	}

	ctrh = (struct ctr_hdr *)skb_push(skb, sizeof(struct ctr_hdr));	// Push LLC Ctrl Head

	ctrh->d_or_c = 1;
	ctrh->ctl_frame_t = type;
	ctrh->len = len;
	ctrh->vehicle_id = llcID;
	ctrh->res1 = 0;
	ctrh->res2 = 0;	
	
	return 0;
}


/* 
 *	Judge if skb is rtp packet
 *	0x00 : ctrl packet
 *	0x02 : need ARQ
 * 	0x06 : RTP packet, need ARQ and collate
 */
llc_traffic_id TrafficID(struct sk_buff *skb)
{
	struct rtphdr * rtph = (struct rtphdr *)(skb_transport_header(skb) + sizeof(struct udphdr));
	struct iphdr * iph = (struct iphdr *)skb_network_header(skb);
	struct udphdr * udph = (struct udphdr *)skb_transport_header(skb);
	unsigned int size = skb->len - sizeof(struct iphdr) - sizeof(struct udphdr);
	struct rtp_buffer * rtpbuf = NULL;
	int i = 0, empty = -1;
	bool comp_success = false;
	static unsigned int rtpnum = 0;

	/* RTP should be UDP packet */
	if(iph->protocol != IPPROTO_UDP) return DATA_AM0;

	/* RTP size should be greater than 12B */
	if(size < 12) return DATA_AM0;

	/* RTP version must be 2 */
	if(rtph->version != 2) return DATA_AM0;

	/* RTP table is empty */
	if(rtpnum == 0)
	{
		rtpbuf = &rtps[0];
		rtpbuf->valid = true;
		rtpbuf->jiffies = jiffies;
		rtpbuf->saddr = iph->saddr;
		rtpbuf->daddr = iph->daddr;		
		rtpbuf->sport = udph->source;
		rtpbuf->dport = udph->dest;
		rtpbuf->pt 	  = rtph->pt;
		rtpbuf->ssrc  = rtph->ssrc;
		rtpbuf->seq   = rtph->seq;
		rtpnum++;
		return DATA_AM0;
	}

	// Compare address and port
	for(i = 0; i < MAX_RTP_NUM; i++)
	{
		rtpbuf = &rtps[i];

		/* Invaild rtp buffer */
		if(!rtpbuf->valid)
		{
			/* Save first empty buffer */
			if(empty < 0) empty = i;
			continue;
		}
		
		/* Compare success */
		if(rtpbuf->saddr == iph->saddr && rtpbuf->daddr == iph->daddr\
					&& rtpbuf->sport == udph->source && rtpbuf->dport == udph->dest)
		{
			comp_success = true;
			break;
		}
					
		/* Release old buffer (live time : 1sec), save first empty buffer */
		if(time_after(jiffies, rtpbuf->jiffies + HZ))
		{
			rtpbuf->valid = false;
			if(empty < 0) empty = i;
			rtpnum -= 1;
		}
	}

	/* Compare success */
	if(comp_success)
	{
		/* Packet Type should be same */
		if(rtph->pt != rtpbuf->pt)	return DATA_AM0;

		/* Synchronization source identifier should be same */
		if(rtph->ssrc != rtpbuf->ssrc)	return DATA_AM0;

		/* Sequence Number should be each increase, set threshold as 5 */
		if(ntohs(rtph->seq) < ntohs(rtpbuf->seq))		return DATA_AM0;
		if(ntohs(rtph->seq) > ntohs(rtpbuf->seq) + 5)	return DATA_AM0;
		
		rtpbuf->jiffies = jiffies;
		rtpbuf->pt 	  = rtph->pt;
		rtpbuf->ssrc  = rtph->ssrc;
		rtpbuf->seq   = rtph->seq;

		return DATA_AM2;
	}

	/* Compare failed */
	/* If buffer is not full, fill the buffer; If buffer is full, do nothing. */
	if(empty >= 0)	
	{
		rtpbuf = &rtps[(unsigned int)empty];
		rtpbuf->valid = true;
		rtpbuf->jiffies = jiffies;
		rtpbuf->saddr = iph->saddr;
		rtpbuf->daddr = iph->daddr;		
		rtpbuf->sport = udph->source;
		rtpbuf->dport = udph->dest;
		rtpbuf->pt 	  = rtph->pt;
		rtpbuf->ssrc  = rtph->ssrc;
		rtpbuf->seq   = rtph->seq;

		rtpnum++;
	}
	return DATA_AM0;
}

void LLC_Data_Init(struct data_hdr *LLC_data_hdr, struct sk_buff *skb)
{
	static u16 LLC_SeqNum = 0;
	static u16 dbg = 0;

	LLC_data_hdr->d_or_c = 0;			// D/C Data
	LLC_data_hdr->ack_flag = 0;			// Ack Off
	LLC_data_hdr->frag_flag = 3;			// No frag
	LLC_data_hdr->llc_id = TrafficID(skb);	
	LLC_data_hdr->res1 = 0;
	LLC_data_hdr->vehicle_id = 0;
	LLC_data_hdr->frag_sn = 0;
	LLC_data_hdr->re_frag_num = 0;
	LLC_data_hdr->re_frag_sn = 0;
	LLC_data_hdr->pkt_sn = (u64)(LLC_SeqNum++);
	LLC_data_hdr->len = skb->len;
	LLC_data_hdr->res3 = 0;

	if(LLC_SeqNum >= 4096) LLC_SeqNum = 1;	// 12bit Seqnum.

	if(LLC_data_hdr->llc_id == DATA_AM2)
	{
		dbg++;
		if(dbg % 50 == 1)
		printk("DATA_AM2 num = %d\n", dbg);
	}
}

/* Copy struct skb's parameters */
void skb_header_copy(struct sk_buff *new, const struct sk_buff *old)
{
	unsigned long offset = skb_headroom(new) - skb_headroom(old);

	new->tstamp		= old->tstamp;
	new->dev		= old->dev;
	new->transport_header	= old->transport_header + offset;
	new->network_header		= old->network_header + offset;
	new->mac_header			= old->mac_header + offset;
	
	skb_dst_copy(new, old);
		
#ifdef CONFIG_XFRM
	new->sp			= secpath_get(old->sp);
#endif

	memcpy(new->cb, old->cb, sizeof(old->cb));
	new->csum			= old->csum;
	new->pkt_type		= old->pkt_type;
	new->ip_summed		= old->ip_summed;
	
	skb_copy_queue_mapping(new, old);

	new->priority		= old->priority;
#if defined(CONFIG_IP_VS) || defined(CONFIG_IP_VS_MODULE)
	new->ipvs_property	= old->ipvs_property;
#endif
	new->protocol		= old->protocol;
	new->mark			= old->mark;
	new->skb_iif		= old->skb_iif;

	__nf_copy(new, old, false);

#if defined(CONFIG_NETFILTER_XT_TARGET_TRACE) || defined(CONFIG_NETFILTER_XT_TARGET_TRACE_MODULE)
	new->nf_trace		= old->nf_trace;
#endif
#ifdef CONFIG_NET_SCHED
	new->tc_index		= old->tc_index;
#ifdef CONFIG_NET_CLS_ACT
	new->tc_verd		= old->tc_verd;
#endif
#endif
	new->vlan_tci		= old->vlan_tci;
	new->secmark		= old->secmark;
}

void SKBS_FREE(struct sk_buff ** skbs, struct sk_buff *skb, int FragNum)
{
	int cnt = 0;
	for(cnt = 0; cnt < FragNum; cnt++)
		kfree_skb(skbs[cnt]);
	kfree_skb(skb);
}

int TOO_MUCH_FRAGMENTS(struct sk_buff ** skbs, struct sk_buff *skb, int FragNum)
{
	printk(KERN_ERR"FragNum > MAX_FRAG_NUM(%d)\n", MAX_FRAG_NUM);
	SKBS_FREE(skbs, skb, FragNum);
	return -1;
}

int SKB_ALLOC_FAILED(struct sk_buff ** skbs, struct sk_buff *skb, int FragNum)
{
	printk(KERN_ERR"skb alloc failed\n");
	SKBS_FREE(skbs, skb, FragNum);
	return -1;
}


struct sk_buff * skb_add_head_list(unsigned int len, struct sk_buff * skb_para)
{
	struct sk_buff *skb = alloc_skb(len, GFP_ATOMIC);
	struct skb_shared_info * shinfo = NULL;
	
	if(unlikely(!skb))
		return NULL;

	skb_reserve(skb, len);
	skb->pkt_type  = PACKET_OTHERHOST;
	
	shinfo = skb_shinfo(skb);
	shinfo->frag_list = skb_para;
	shinfo->nr_frags = 0;
	shinfo->gso_segs = 0;
	shinfo->gso_size = 0;
	shinfo->gso_type = SKB_GSO_UDP;
	skb->len = skb_para->len;
	skb->data_len = skb_para->len;
	skb->truesize += skb_para->len;

	return skb;
}


