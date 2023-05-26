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
#include <asm/mwait.h>

#define TOTAL_ENERGY_CONSUMED_MASK (0xffffffff)

#define MAX_NUMBER_OF_MEASUREMENTS (1000)

MODULE_LICENSE("GPL");

// settings
static int measurement_duration = 100;
module_param(measurement_duration, int, 0);
MODULE_PARM_DESC(Measurement_duration, "Duration of each measurement in milliseconds.");
static int measurement_count = 10;
module_param(measurement_count, int, 0);
MODULE_PARM_DESC(measurement_count, "How many measurements should be done.");
static int cpus_mwait = -1;
module_param(cpus_mwait, int, 0);
MODULE_PARM_DESC(cpus_mwait, "Number of CPUs that should do mwait instead of a busy loop during the measurement.");
static char *cpu_selection = "core";
module_param(cpu_selection, charp, 0);
MODULE_PARM_DESC(cpu_selection, "How the cpus doing mwait should be selected. Supported are 'core' and 'cpu_nr'.");
static int target_cstate = 0;
module_param(target_cstate, int, 0);
MODULE_PARM_DESC(target_cstate, "The C-State that gets passed to mwait as a hint.");
static int target_subcstate = 0;
module_param(target_subcstate, int, 0);
MODULE_PARM_DESC(target_subcstate, "The sub C-State that gets passed to mwait as a hint.");

static u64 measurement_results[MAX_NUMBER_OF_MEASUREMENTS];

DEFINE_PER_CPU(int, trigger);
static u64 start_rapl;
static u64 consumed_energy;
static volatile int dummy;
static atomic_t sync_var;
static bool redo_measurement;

static u32 rapl_unit;
static unsigned cpus_present;
static u32 mwait_hint;
static int apic_id_of_cpu0;
static int hpet_pin;

static ssize_t show_measurement_results(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t ignore_write(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);

struct kobject *kobj_ref;
struct kobj_attribute measurement_results_attr =
    __ATTR(measurement_results, 0444, show_measurement_results, ignore_write);

static ssize_t format_array_into_buffer(u64 *array, char *buf)
{
    int bytes_written = 0;
    int i = 0;

    while (bytes_written < PAGE_SIZE && i < measurement_count)
    {
        bytes_written += scnprintf(buf + bytes_written, PAGE_SIZE - bytes_written, "%llu\n", array[i]);
        ++i;
    }
    return bytes_written;
}

static ssize_t show_measurement_results(struct kobject *kobj,
                                        struct kobj_attribute *attr, char *buf)
{
    return format_array_into_buffer(measurement_results, buf);
}

static ssize_t ignore_write(struct kobject *kobj, struct kobj_attribute *attr,
                            const char *buf, size_t count)
{
    return count;
}

static int nmi_handler(unsigned int val, struct pt_regs *regs)
{
    int this_cpu = smp_processor_id();

    if (!this_cpu)
    {
        u64 final_rapl;
        rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &final_rapl);
        final_rapl &= TOTAL_ENERGY_CONSUMED_MASK;
        consumed_energy = final_rapl - start_rapl;
        if (final_rapl <= start_rapl)
        {
            printk(KERN_INFO "Result would have been %llu, redoing measurement!\n", consumed_energy);
            redo_measurement = 1;
        }
        else
        {
            printk(KERN_INFO "Consumed Energy: %llu\n", consumed_energy);
        }

        apic->send_IPI_allbutself(NMI_VECTOR);
    }

    per_cpu(trigger, this_cpu) = 0;

    return NMI_HANDLED;
}

static inline void wait_for_rapl_update(void)
{
    u64 original_value;
    rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &original_value);
    original_value &= TOTAL_ENERGY_CONSUMED_MASK;
    do
    {
        rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &start_rapl);
        start_rapl &= TOTAL_ENERGY_CONSUMED_MASK;
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
        setup_hpet_for_measurement(measurement_duration, hpet_pin);
        atomic_inc(&sync_var);
    }
    else
    {
        while (atomic_read(&sync_var) < cpus_present + 1)
        {
        }
    }
}

static void do_idle_loop(int this_cpu)
{
    per_cpu(trigger, this_cpu) = 1;

    sync(this_cpu);

    while (per_cpu(trigger, this_cpu))
    {
    }

    printk(KERN_INFO "CPU %i: Waking up\n", this_cpu);
}

static void do_mwait(int this_cpu)
{
    per_cpu(trigger, this_cpu) = 1;

    sync(this_cpu);

    while (per_cpu(trigger, this_cpu))
    {
        asm volatile("monitor;" ::"a"(&dummy), "c"(0), "d"(0));
        asm volatile("mwait;" ::"a"(mwait_hint), "c"(0));
    }

    printk(KERN_INFO "CPU %i: Waking up\n", this_cpu);
}

static bool should_do_mwait(int this_cpu)
{
    if (strcmp(cpu_selection, "cpu_nr") == 0)
    {
        return this_cpu < cpus_mwait;
    }

    return (this_cpu < cpus_present / 2
                ? 2 * this_cpu
                : (this_cpu - (cpus_present / 2)) * 2 + 1) < cpus_mwait;
}

static void per_cpu_func(void *info)
{
    int this_cpu = get_cpu();
    local_irq_disable();

    if (should_do_mwait(this_cpu))
    {
        do_mwait(this_cpu);
    }
    else
    {
        do_idle_loop(this_cpu);
    }

    local_irq_enable();
    put_cpu();
}

static void measure(unsigned long long *result)
{
    do
    {
        redo_measurement = 0;
        atomic_set(&sync_var, 0);
        on_each_cpu(per_cpu_func, NULL, 1);
        *result = consumed_energy * rapl_unit;

        restore_hpet_after_measurement();
    } while (redo_measurement);
}

// Get the unit of the PKG_ENERGY_STATUS MSR in 0.1 microJoule
static inline u32 get_rapl_unit(void)
{
    u64 val;
    rdmsrl_safe(MSR_RAPL_POWER_UNIT, &val);
    val = (val >> 8) & 0b11111;
    return 10000000 / (1 << val);
}

static int mwait_init(void)
{
    u32 a = 0x1, b, c, d;
    asm("cpuid;"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "0"(a));
    if (!(c & 0b1000))
    {
        printk(KERN_ERR "WARNING: Mwait not supported.\n");
    }

    a = 0x5;
    asm("cpuid;"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "0"(a));
    if (!(c & 0b1))
    {
        printk(KERN_ERR "WARNING: Mwait Power Management not supported.\n");
    }

    mwait_hint = 0;
    mwait_hint += target_subcstate & MWAIT_SUBSTATE_MASK;
    mwait_hint += (target_cstate & MWAIT_CSTATE_MASK) << MWAIT_SUBSTATE_SIZE;
    printk(KERN_INFO "Using MWAIT hint 0x%x.", mwait_hint);

    measurement_count = measurement_count < MAX_NUMBER_OF_MEASUREMENTS
                            ? measurement_count
                            : MAX_NUMBER_OF_MEASUREMENTS;

    rapl_unit = get_rapl_unit();
    printk(KERN_INFO "rapl_unit in 0.1 microJoule: %u\n", rapl_unit);

    cpus_present = num_present_cpus();
    if (cpus_mwait == -1)
        cpus_mwait = cpus_present;

    register_nmi_handler(NMI_UNKNOWN, nmi_handler, 0, "nmi_handler");

    apic_id_of_cpu0 = default_cpu_present_to_apicid(0);

    hpet_pin = select_hpet_pin();
    if (hpet_pin == -1)
    {
        printk(KERN_ERR "ERROR: No suitable pin found for HPET, aborting!\n");
        return 1;
    }

    setup_ioapic_for_measurement(apic_id_of_cpu0, hpet_pin);

    for (int i = 0; i < measurement_count; ++i)
    {
        measure(&(measurement_results[i]));
    }

    restore_ioapic_after_measurement();

    kobj_ref = kobject_create_and_add("mwait_measurements", NULL);
    if (sysfs_create_file(kobj_ref, &measurement_results_attr.attr))
    {
        printk(KERN_ERR "ERROR: Could not create sysfs file, aborting!\n");
        sysfs_remove_file(kobj_ref, &measurement_results_attr.attr);
        kobject_put(kobj_ref);
        return 1;
    }

    printk(KERN_INFO "Successfully initialized MWAIT kernel module.\n");

    return 0;
}

static void mwait_exit(void)
{
    sysfs_remove_file(kobj_ref, &measurement_results_attr.attr);
    kobject_put(kobj_ref);

    unregister_nmi_handler(NMI_UNKNOWN, "nmi_handler");
    printk(KERN_INFO "Successfully exited MWAIT kernel module.\n");
}

module_init(mwait_init);
module_exit(mwait_exit);
