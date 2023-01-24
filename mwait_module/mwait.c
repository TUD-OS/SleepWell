#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/apic.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");

volatile struct {
    char padding[8192]; 
    int trigger;
    char padding2[8192];
} test;

void test_function(void* info){
    int this_cpu = get_cpu();

    local_irq_disable();
    if(this_cpu) {
        for(int i = 0; i<10; ++i) {
            
            asm volatile("monitor;"
		     :: "a" (&test.trigger), "c" (0), "d"(0));
            asm volatile("mwait;"
		     :: "a" (0), "c" (0));

            printk(KERN_INFO "CPU %i: Iteration %i, trigger value: %i\n", this_cpu, i, test.trigger);
        }
    } else {
        for(int i = 0; i<10; ++i) {
            mdelay(1000);
            printk(KERN_INFO "CPU %i: Triggering iteration %i, trigger value: %i\n", this_cpu, i, i);
            test.trigger = i;
        }
    }
    local_irq_enable();
}

bool cond_function(int cpu, void *info) {
    return cpu < 4;
}

static int myinit(void)
{
    printk(KERN_INFO "mwait init\n");
    test.trigger = -1;

    int a = 0x1, b, c, d;
    asm ( "cpuid;"
        : "=a" (a), "=b" (b), "=c" (c), "=d" (d)
        : "0" (a)
    );
    if(!(c & 0b1000)) {
        printk(KERN_INFO "Mwait not supported.\n");
        return 0;
    }
    printk(KERN_INFO "Mwait supported, continuing.\n");

    unsigned int cpus_online = num_online_cpus();
    printk(KERN_INFO "Online Cpus: %i\n", cpus_online);

    on_each_cpu_cond(cond_function, test_function, NULL, 1);

    return 0;
}

static void myexit(void)
{
    printk(KERN_INFO "mwait exit\n");
}

module_init(myinit)
module_exit(myexit)
