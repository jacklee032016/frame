/* 
 *
 */
#include "testBaseTest.h"
#include <linux/module.h>
#include <linux/kernel.h>

int init_module(void)
{
	printk(KERN_INFO " test module loaded. Starting tests...\n");

	testBaseMain();

	/* Prevent module from loading. We've finished test anyway.. */
	return 1;
}

void cleanup_module(void)
{
	printk(KERN_INFO " test module unloading...\n");
}

MODULE_LICENSE("GPL");

