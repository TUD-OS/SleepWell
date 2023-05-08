#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/apic.h>
#include <asm/msr.h>
#include <asm/hw_irq.h>
#include <asm/nmi.h>
#include <linux/delay.h>
#include <asm/msr-index.h>
#include <asm/io_apic.h>
#include <asm/hpet.h>

MODULE_LICENSE("GPL");

DEFINE_PER_CPU(int, trigger);
unsigned long long start_rapl;
volatile int dummy;

bool cond_function(int cpu, void *info)
{
    return 1;
}

int nmi_handler(unsigned int val, struct pt_regs* regs) {
    int this_cpu = smp_processor_id();

    if(!this_cpu) {
        unsigned long long final_rapl;
        rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &final_rapl);
        printk("Consumed Energy: %llu\n", final_rapl - start_rapl);

        apic->send_IPI_allbutself(NMI_VECTOR);
    }

    per_cpu(trigger, this_cpu) = 0;

    return NMI_HANDLED;
}

void measure_nop(void* info) {
    int this_cpu = get_cpu();
    per_cpu(trigger, this_cpu) = 1;

    local_irq_disable();

    while(per_cpu(trigger, this_cpu)) {}

    printk(KERN_INFO "CPU %i: Waking up\n", this_cpu);

    local_irq_enable();
}

void measure_mwait(void* info) {
    int this_cpu = get_cpu();

    local_irq_disable();

    asm volatile("monitor;" ::"a"(&dummy), "c"(0), "d"(0));
    asm volatile("mwait;" ::"a"(0), "c"(0));

    printk(KERN_INFO "CPU %i: Waking up\n", this_cpu);

    local_irq_enable();
}

static int mwait_init(void)
{
    printk(KERN_INFO "mwait init\n");

    int a = 0x1, b, c, d;
    asm("cpuid;"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "0"(a));
    if (!(c & 0b1000))
    {
        printk(KERN_INFO "Mwait not supported.\n");
        return 0;
    }
    printk(KERN_INFO "Mwait supported, continuing.\n");

    unsigned cpus_online = num_online_cpus();
    printk(KERN_INFO "Online Cpus: %i\n", cpus_online);

    register_nmi_handler(NMI_UNKNOWN, nmi_handler, 0, "nmi_handler");

    int apic_id = default_cpu_present_to_apicid(0);

    int pin = select_hpet_pin();
    if(pin == -1) {
        printk(KERN_INFO "HPET: No suitable pin found, aborting!\n");
        return 0;
    }
    printk(KERN_INFO "HPET: Pin %u selected!\n", pin);
    setup_ioapic_for_measurement(apic_id, pin);

    unsigned long long original_value;
    rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &original_value);
    do {
        rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &start_rapl);
    } while (original_value == start_rapl);

    setup_hpet_for_measurement(1000, pin);

    on_each_cpu_cond(cond_function, measure_mwait, NULL, 1);

    rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &original_value);
    do {
        rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &start_rapl);
    } while (original_value == start_rapl);

    setup_hpet_for_measurement(1000, pin);

    on_each_cpu_cond(cond_function, measure_nop, NULL, 1);

    restore_hpet_after_measurement();
    restore_ioapic_after_measurement();

    return 0;
}

static void mwait_exit(void)
{
    unregister_nmi_handler(NMI_UNKNOWN, "nmi_handler");
    printk(KERN_INFO "mwait exit\n");
}

module_init(mwait_init)
module_exit(mwait_exit)
