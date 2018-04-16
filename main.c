#include "source.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NIC test");
MODULE_AUTHOR("MAC");

static struct nf_hook_ops postRoutHook;

//struct dentry * my_debugfs_root = NULL;
//
//struct debugfs_blob_wrapper arraydata;
//struct dentry * my_debugfs = NULL;
//
//struct dentry * my_debugfs_file = NULL;
//static const struct file_operations user_net_fops
//{
//.owner = THIS_MODULE,
//.open = seq_open,
//.read = seq_read,
//.write = seq_write,
//.llseek = seq_lseek,
//.release = single_release,
//};




int mmHookInit(void)
{
	postRoutHook.hook = postRoutHookDisp;	
	postRoutHook.hooknum = NF_INET_POST_ROUTING;	// Hook data in "post routing" position
	postRoutHook.pf = NFPROTO_IPV4;
	postRoutHook.priority = NF_IP_PRI_LAST;
	nf_register_hook(&postRoutHook);
	return 0;
}

void mmHookExit(void)
{
	nf_unregister_hook(&postRoutHook);
}


static int mm_init(void)
{	
	Link_Init(&_dev_to_link, &_local_adapter);	// Update link information
	mmHookInit();
	memset(rtps, 0, sizeof(rtps));	
	printk("terminal start work!\n");
	return 0;
}


static void mm_exit(void)
{
	msleep(1000);

//	debugfs_remove_recursive(my_debugfs_root);
//	my_debugfs_root = NULL;
	
	mmHookExit();
	mmDebug(MM_INFO,"xmit_Exit run..\n");
}

module_init(mm_init);
module_exit(mm_exit);
