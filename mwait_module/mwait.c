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

#define NUMBER_OF_MEASUREMENTS (10)

MODULE_LICENSE("GPL");

// settings
static int duration = 100;
module_param(duration, int, 0);
MODULE_PARM_DESC(duration, "Duration of each measurement in milliseconds.");

// results
static unsigned long long idle_loop_consumed_energy[NUMBER_OF_MEASUREMENTS];
static unsigned long long mwait_consumed_energy[NUMBER_OF_MEASUREMENTS];

DEFINE_PER_CPU(int, trigger);
static unsigned long long start_rapl;
static unsigned long long consumed_energy;
static volatile int dummy;
static atomic_t sync_var;
static bool redo_measurement;

static unsigned cpus_present;
static int apic_id_of_cpu0;
static int hpet_pin;

static ssize_t show_idle_loop_consumed_energy(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t show_mwait_consumed_energy(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t ignore_write(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);

struct kobject *kobj_ref;
struct kobj_attribute idle_loop_measurement =
    __ATTR(idle_loop_consumed_energy, 0444, show_idle_loop_consumed_energy, ignore_write);
struct kobj_attribute mwait_measurement =
    __ATTR(mwait_consumed_energy, 0444, show_mwait_consumed_energy, ignore_write);

static ssize_t format_array_into_buffer(unsigned long long *array, char *buf)
{
    int bytes_written = 0;
    int i = 0;

    while (bytes_written < PAGE_SIZE && i < NUMBER_OF_MEASUREMENTS)
    {
        bytes_written += scnprintf(buf + bytes_written, PAGE_SIZE - bytes_written, "%llu\n", array[i]);
        ++i;
    }
    return bytes_written;
}

static ssize_t show_idle_loop_consumed_energy(struct kobject *kobj,
                                              struct kobj_attribute *attr, char *buf)
{
    return format_array_into_buffer(idle_loop_consumed_energy, buf);
}

static ssize_t show_mwait_consumed_energy(struct kobject *kobj,
                                          struct kobj_attribute *attr, char *buf)
{
    return format_array_into_buffer(mwait_consumed_energy, buf);
}

static ssize_t ignore_write(struct kobject *kobj, struct kobj_attribute *attr,
                            const char *buf, size_t count)
{
    return count;
}

static bool cond_function(int cpu, void *info)
{
    return 1;
}

static int nmi_handler(unsigned int val, struct pt_regs *regs)
{
    int this_cpu = smp_processor_id();

    if (!this_cpu)
    {
        unsigned long long final_rapl;
        rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &final_rapl);
        consumed_energy = final_rapl - start_rapl;
        if(final_rapl <= start_rapl) {
            printk(KERN_INFO "Result would have been %llu, redoing measurement!\n", consumed_energy);
            redo_measurement = 1;
        } else {
            printk(KERN_INFO "Consumed Energy: %llu\n", consumed_energy);
        }

        apic->send_IPI_allbutself(NMI_VECTOR);
    }

    per_cpu(trigger, this_cpu) = 0;

    return NMI_HANDLED;
}

static inline void wait_for_rapl_update(void)
{
    unsigned long long original_value;
    rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &original_value);
    do
    {
        rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &start_rapl);
    } while (original_value == start_rapl);
}

static inline void sync(int this_cpu)
{
    atomic_inc(&sync_var);
    if (!this_cpu)
    {
        while (atomic_read(&sync_var) < cpus_present)
        {
        }
        wait_for_rapl_update();
        setup_hpet_for_measurement(duration, hpet_pin);
        atomic_inc(&sync_var);
    }
    else
    {
        while (atomic_read(&sync_var) < cpus_present + 1)
        {
        }
    }
}

static void do_idle_loop(void *info)
{
    int this_cpu = get_cpu();
    local_irq_disable();

    per_cpu(trigger, this_cpu) = 1;

    sync(this_cpu);

    while (per_cpu(trigger, this_cpu))
    {
    }

    printk(KERN_INFO "CPU %i: Waking up\n", this_cpu);

    local_irq_enable();
    put_cpu();
}

static void do_mwait(void *info)
{
    int this_cpu = get_cpu();
    local_irq_disable();

    sync(this_cpu);

    asm volatile("monitor;" ::"a"(&dummy), "c"(0), "d"(0));
    asm volatile("mwait;" ::"a"(0), "c"(0));

    printk(KERN_INFO "CPU %i: Waking up\n", this_cpu);

    local_irq_enable();
    put_cpu();
}

static void measure(smp_call_func_t func, unsigned long long *result)
{
    do {
        redo_measurement = 0;
        atomic_set(&sync_var, 0);
        on_each_cpu_cond(cond_function, func, NULL, 1);
        *result = consumed_energy;

        restore_hpet_after_measurement();
    } while (redo_measurement);
}

static int mwait_init(void)
{
    int a = 0x1, b, c, d;
    asm("cpuid;"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "0"(a));
    if (!(c & 0b1000))
    {
        printk(KERN_WARNING "WARNING: Mwait not supported.\n");
    }

    cpus_present = num_present_cpus();

    register_nmi_handler(NMI_UNKNOWN, nmi_handler, 0, "nmi_handler");

    apic_id_of_cpu0 = default_cpu_present_to_apicid(0);

    hpet_pin = select_hpet_pin();
    if (hpet_pin == -1)
    {
        printk(KERN_ERR "ERROR: No suitable pin found for HPET, aborting!\n");
        return 1;
    }

    setup_ioapic_for_measurement(apic_id_of_cpu0, hpet_pin);

    for (int i = 0; i < NUMBER_OF_MEASUREMENTS; ++i)
    {
        measure(do_mwait, &(mwait_consumed_energy[i]));
        measure(do_idle_loop, &(idle_loop_consumed_energy[i]));
    }

    restore_ioapic_after_measurement();

    kobj_ref = kobject_create_and_add("mwait_measurements", NULL);
    if (sysfs_create_file(kobj_ref, &idle_loop_measurement.attr) ||
        sysfs_create_file(kobj_ref, &mwait_measurement.attr))
    {
        printk(KERN_ERR "ERROR: Could not create sysfs files, aborting\n");
        sysfs_remove_file(kobj_ref, &idle_loop_measurement.attr);
        sysfs_remove_file(kobj_ref, &mwait_measurement.attr);
        kobject_put(kobj_ref);
    }

    printk(KERN_INFO "Successfully initialized MWAIT kernel module.\n");

    return 0;
}

static void mwait_exit(void)
{
    sysfs_remove_file(kobj_ref, &idle_loop_measurement.attr);
    sysfs_remove_file(kobj_ref, &mwait_measurement.attr);
    kobject_put(kobj_ref);

    unregister_nmi_handler(NMI_UNKNOWN, "nmi_handler");
    printk(KERN_INFO "Successfully exited MWAIT kernel module.\n");
}

module_init(mwait_init);
module_exit(mwait_exit);
