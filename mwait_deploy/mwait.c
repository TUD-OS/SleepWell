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
#include <linux/sched/clock.h>

#define TOTAL_ENERGY_CONSUMED_MASK (0xffffffff)
#define IA32_FIXED_CTR2 (0x30b)
#define IA32_FIXED_CTR_CTRL (0x38d)
#define IA32_PERF_GLOBAL_CTRL (0x38f)

#define MAX_NUMBER_OF_MEASUREMENTS (1000)
#define MAX_CPUS (32)

MODULE_LICENSE("GPL");

// settings
static int measurement_duration = 100;
module_param(measurement_duration, int, 0);
MODULE_PARM_DESC(measurement_duration, "Duration of each measurement in milliseconds.");
static int measurement_count = 10;
module_param(measurement_count, int, 0);
MODULE_PARM_DESC(measurement_count, "How many measurements should be done.");
static int cpus_mwait = -1;
module_param(cpus_mwait, int, 0);
MODULE_PARM_DESC(cpus_mwait, "Number of CPUs that should do mwait instead of a busy loop during the measurement.");
static char *cpu_selection = "core";
module_param(cpu_selection, charp, 0);
MODULE_PARM_DESC(cpu_selection, "How the cpus doing mwait should be selected. Supported are 'core' and 'cpu_nr'.");
static char *mwait_hint = "";
module_param(mwait_hint, charp, 0);
MODULE_PARM_DESC(mwait_hint, "The hint mwait should use. If this is given, target_cstate and target_subcstate are ignored.");
static int target_cstate = 1;
module_param(target_cstate, int, 0);
MODULE_PARM_DESC(target_cstate, "The C-State that gets passed to mwait as a hint.");
static int target_subcstate = 0;
module_param(target_subcstate, int, 0);
MODULE_PARM_DESC(target_subcstate, "The sub C-State that gets passed to mwait as a hint.");

static struct pkg_stat
{
    struct kobject kobject;
    u64 energy_consumption[MAX_NUMBER_OF_MEASUREMENTS];
    u64 total_tsc[MAX_NUMBER_OF_MEASUREMENTS];
    u64 wakeup_time[MAX_NUMBER_OF_MEASUREMENTS];
    u64 c2[MAX_NUMBER_OF_MEASUREMENTS];
    u64 c3[MAX_NUMBER_OF_MEASUREMENTS];
    u64 c6[MAX_NUMBER_OF_MEASUREMENTS];
    u64 c7[MAX_NUMBER_OF_MEASUREMENTS];
} pkg_stats;

static struct cpu_stat
{
    struct kobject kobject;
    u64 wakeups[MAX_NUMBER_OF_MEASUREMENTS];
    u64 unhalted[MAX_NUMBER_OF_MEASUREMENTS];
    u64 c3[MAX_NUMBER_OF_MEASUREMENTS];
    u64 c6[MAX_NUMBER_OF_MEASUREMENTS];
    u64 c7[MAX_NUMBER_OF_MEASUREMENTS];
} cpu_stats[MAX_CPUS];

DEFINE_PER_CPU(u64, wakeups);
DEFINE_PER_CPU(u64, start_unhalted);
DEFINE_PER_CPU(u64, final_unhalted);
DEFINE_PER_CPU(u64, start_c3);
DEFINE_PER_CPU(u64, final_c3);
DEFINE_PER_CPU(u64, start_c6);
DEFINE_PER_CPU(u64, final_c6);
DEFINE_PER_CPU(u64, start_c7);
DEFINE_PER_CPU(u64, final_c7);
static u64 start_time, final_time;
static u64 start_rapl, final_rapl;
static u64 start_tsc, final_tsc;
static u64 start_pkg_c2, final_pkg_c2;
static u64 start_pkg_c3, final_pkg_c3;
static u64 start_pkg_c6, final_pkg_c6;
static u64 start_pkg_c7, final_pkg_c7;
static u64 hpet_comparator, hpet_counter, wakeup_time;

DEFINE_PER_CPU(int, trigger);
static volatile int dummy;
static atomic_t sync_var;
static bool redo_measurement;
static bool end_of_measurement;
static int first;

static u32 cpu_model;
static u32 cpu_family;
static u32 rapl_unit;
static unsigned cpus_present;
static u32 calculated_mwait_hint;
static int apic_id_of_cpu0;
static int hpet_pin;
static u32 hpet_period;

static struct attribute pkg_energy_consumption_attribute = {
    .name = "pkg_energy_consumption",
    .mode = 0444};
static struct attribute total_tsc_attribute = {
    .name = "total_tsc",
    .mode = 0444};
static struct attribute wakeup_time_attribute = {
    .name = "wakeup_time",
    .mode = 0444};
static struct attribute pkg_c2_attribute = {
    .name = "pkg_c2",
    .mode = 0444};
static struct attribute pkg_c3_attribute = {
    .name = "pkg_c3",
    .mode = 0444};
static struct attribute pkg_c6_attribute = {
    .name = "pkg_c6",
    .mode = 0444};
static struct attribute pkg_c7_attribute = {
    .name = "pkg_c7",
    .mode = 0444};
static struct attribute *pkg_stats_attributes[] = {
    &pkg_energy_consumption_attribute,
    &total_tsc_attribute,
    &wakeup_time_attribute,
    &pkg_c2_attribute,
    &pkg_c3_attribute,
    &pkg_c6_attribute,
    &pkg_c7_attribute,
    NULL};
static struct attribute_group pkg_stats_group = {
    .attrs = pkg_stats_attributes};
static const struct attribute_group *pkg_stats_groups[] = {
    &pkg_stats_group,
    NULL};
static struct attribute wakeups_attribute = {
    .name = "wakeups",
    .mode = 0444};
static struct attribute unhalted_attribute = {
    .name = "unhalted",
    .mode = 0444};
static struct attribute c3_attribute = {
    .name = "c3",
    .mode = 0444};
static struct attribute c6_attribute = {
    .name = "c6",
    .mode = 0444};
static struct attribute c7_attribute = {
    .name = "c7",
    .mode = 0444};
static struct attribute *cpu_stats_attributes[] = {
    &wakeups_attribute,
    &unhalted_attribute,
    &c3_attribute,
    &c6_attribute,
    &c7_attribute,
    NULL};
static struct attribute_group cpu_stats_group = {
    .attrs = cpu_stats_attributes};
static const struct attribute_group *cpu_stats_groups[] = {
    &cpu_stats_group,
    NULL};

static ssize_t show_cpu_stats(struct kobject *kobj, struct attribute *attr, char *buf);
static ssize_t show_pkg_stats(struct kobject *kobj, struct attribute *attr, char *buf);
static ssize_t ignore_write(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
    return count;
}
static void release(struct kobject *kobj) {}

static const struct sysfs_ops pkg_sysfs_ops = {
    .show = show_pkg_stats,
    .store = ignore_write};
static const struct sysfs_ops cpu_sysfs_ops = {
    .show = show_cpu_stats,
    .store = ignore_write};
static const struct kobj_type pkg_ktype = {
    .sysfs_ops = &pkg_sysfs_ops,
    .release = release,
    .default_groups = pkg_stats_groups};
static const struct kobj_type cpu_ktype = {
    .sysfs_ops = &cpu_sysfs_ops,
    .release = release,
    .default_groups = cpu_stats_groups};

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

static ssize_t show_pkg_stats(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct pkg_stat *stat = container_of(kobj, struct pkg_stat, kobject);
    if (strcmp(attr->name, "pkg_energy_consumption") == 0)
        return format_array_into_buffer(stat->energy_consumption, buf);
    if (strcmp(attr->name, "total_tsc") == 0)
        return format_array_into_buffer(stat->total_tsc, buf);
    if (strcmp(attr->name, "wakeup_time") == 0)
        return format_array_into_buffer(stat->wakeup_time, buf);
    if (strcmp(attr->name, "pkg_c2") == 0)
        return format_array_into_buffer(stat->c2, buf);
    if (strcmp(attr->name, "pkg_c3") == 0)
        return format_array_into_buffer(stat->c3, buf);
    if (strcmp(attr->name, "pkg_c6") == 0)
        return format_array_into_buffer(stat->c6, buf);
    if (strcmp(attr->name, "pkg_c7") == 0)
        return format_array_into_buffer(stat->c7, buf);
    return 0;
}

static ssize_t show_cpu_stats(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct cpu_stat *stat = container_of(kobj, struct cpu_stat, kobject);
    if (strcmp(attr->name, "wakeups") == 0)
        return format_array_into_buffer(stat->wakeups, buf);
    if (strcmp(attr->name, "unhalted") == 0)
        return format_array_into_buffer(stat->unhalted, buf);
    if (strcmp(attr->name, "c3") == 0)
        return format_array_into_buffer(stat->c3, buf);
    if (strcmp(attr->name, "c6") == 0)
        return format_array_into_buffer(stat->c6, buf);
    if (strcmp(attr->name, "c7") == 0)
        return format_array_into_buffer(stat->c7, buf);
    return 0;
}

static inline void set_global_final_values(void)
{
    rdmsrl_safe(MSR_PKG_ENERGY_STATUS, &final_rapl);
    final_time = local_clock();
    final_tsc = rdtsc();
    rdmsrl_safe(MSR_PKG_C2_RESIDENCY, &final_pkg_c2);
    rdmsrl_safe(MSR_PKG_C3_RESIDENCY, &final_pkg_c3);
    rdmsrl_safe(MSR_PKG_C6_RESIDENCY, &final_pkg_c6);
    rdmsrl_safe(MSR_PKG_C7_RESIDENCY, &final_pkg_c7);
}

static inline void set_cpu_final_values(int this_cpu)
{
    rdmsrl_safe(IA32_FIXED_CTR2, &per_cpu(final_unhalted, this_cpu));
    rdmsrl_safe(MSR_CORE_C3_RESIDENCY, &per_cpu(final_c3, this_cpu));
    rdmsrl_safe(MSR_CORE_C6_RESIDENCY, &per_cpu(final_c6, this_cpu));
    rdmsrl_safe(MSR_CORE_C7_RESIDENCY, &per_cpu(final_c7, this_cpu));
}

static inline void evaluate_global(void)
{
    final_rapl &= TOTAL_ENERGY_CONSUMED_MASK;
    if (final_rapl <= start_rapl)
    {
        printk(KERN_ERR "Result would have been %llu.\n", final_rapl - start_rapl);
        redo_measurement = 1;
    }
    final_rapl -= start_rapl;
    final_time -= start_time;
    if (final_time < measurement_duration * 1000000)
    {
        printk(KERN_ERR "Measurement lasted only %llu ns.\n", final_time);
        redo_measurement = 1;
    }
    wakeup_time = ((hpet_counter - hpet_comparator) * hpet_period) / 1000000;
    final_tsc -= start_tsc;
    final_pkg_c2 -= start_pkg_c2;
    final_pkg_c3 -= start_pkg_c3;
    final_pkg_c6 -= start_pkg_c6;
    final_pkg_c7 -= start_pkg_c7;

    if (redo_measurement)
    {
        printk(KERN_ERR "Redoing Measurement!\n");
    }
}

static inline void evaluate_cpu(int this_cpu)
{
    per_cpu(final_unhalted, this_cpu) -= per_cpu(start_unhalted, this_cpu);
    per_cpu(final_c3, this_cpu) -= per_cpu(start_c3, this_cpu);
    per_cpu(final_c6, this_cpu) -= per_cpu(start_c6, this_cpu);
    per_cpu(final_c7, this_cpu) -= per_cpu(start_c7, this_cpu);
}

static bool is_cpu_model(u32 family, u32 model) {
    return cpu_family == family && cpu_model == model;
}

static int nmi_handler(unsigned int val, struct pt_regs *regs)
{
    // this measurement is taken here to get the value as early as possible
    u64 hpet_counter_local = get_hpet_counter();

    int this_cpu = smp_processor_id();

    if (!this_cpu)
    {
        if (!first && is_cpu_model(0x6, 0x5e))
        {
            ++first;
            return NMI_HANDLED;
        }

        // only commit the taken time to the global variable if this point is reached
        hpet_counter = hpet_counter_local;

        end_of_measurement = 1;
        apic->send_IPI_allbutself(NMI_VECTOR);

        set_global_final_values();
    }

    set_cpu_final_values(this_cpu);

    if (!end_of_measurement)
    {
        printk(KERN_ERR "CPU %i received unexpected NMI during measurement.\n", this_cpu);
        redo_measurement = 1;
    }

    per_cpu(trigger, this_cpu) = 0;

    // Without some delay here, CPUs tend to get stuck on rare occasions
    // I don't know yet why exactly this happens, so this udelay should be seen as a (hopefully temporary) workaround
    udelay(1);

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

static inline void set_global_start_values(void)
{
    start_time = local_clock();
    start_tsc = rdtsc();
    rdmsrl_safe(MSR_PKG_C2_RESIDENCY, &start_pkg_c2);
    rdmsrl_safe(MSR_PKG_C3_RESIDENCY, &start_pkg_c3);
    rdmsrl_safe(MSR_PKG_C6_RESIDENCY, &start_pkg_c6);
    rdmsrl_safe(MSR_PKG_C7_RESIDENCY, &start_pkg_c7);
}

static inline void set_cpu_start_values(int this_cpu)
{
    rdmsrl_safe(IA32_FIXED_CTR2, &per_cpu(start_unhalted, this_cpu));
    rdmsrl_safe(MSR_CORE_C3_RESIDENCY, &per_cpu(start_c3, this_cpu));
    rdmsrl_safe(MSR_CORE_C6_RESIDENCY, &per_cpu(start_c6, this_cpu));
    rdmsrl_safe(MSR_CORE_C7_RESIDENCY, &per_cpu(start_c7, this_cpu));
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
        set_global_start_values();
        atomic_inc(&sync_var);
        set_cpu_start_values(this_cpu);
        hpet_comparator = setup_hpet_for_measurement(measurement_duration, hpet_pin);
        atomic_inc(&sync_var);
    }
    else
    {
        while (atomic_read(&sync_var) < cpus_present + 1)
        {
        }
        set_cpu_start_values(this_cpu);
        while (atomic_read(&sync_var) < cpus_present + 2)
        {
        }
    }
}

static void do_idle_loop(int this_cpu)
{
    sync(this_cpu);

    while (per_cpu(trigger, this_cpu))
    {
    }
}

static void do_mwait(int this_cpu)
{
    sync(this_cpu);

    while (per_cpu(trigger, this_cpu))
    {
        asm volatile("monitor;" ::"a"(&dummy), "c"(0), "d"(0));
        asm volatile("mwait;" ::"a"(calculated_mwait_hint), "c"(0));
        per_cpu(wakeups, this_cpu) += 1;
    }
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

    per_cpu(trigger, this_cpu) = 1;

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

static inline void commit_results(unsigned number)
{
    pkg_stats.energy_consumption[number] = final_rapl * rapl_unit;
    pkg_stats.total_tsc[number] = final_tsc;
    pkg_stats.wakeup_time[number] = wakeup_time;
    pkg_stats.c2[number] = final_pkg_c2;
    pkg_stats.c3[number] = final_pkg_c3;
    pkg_stats.c6[number] = final_pkg_c6;
    pkg_stats.c7[number] = final_pkg_c7;

    for (unsigned i = 0; i < cpus_present; ++i)
    {
        cpu_stats[i].wakeups[number] = per_cpu(wakeups, i);
        cpu_stats[i].unhalted[number] = per_cpu(final_unhalted, i);
        cpu_stats[i].c3[number] = per_cpu(final_c3, i);
        cpu_stats[i].c6[number] = per_cpu(final_c6, i);
        cpu_stats[i].c7[number] = per_cpu(final_c7, i);
    }
}

static void measure(unsigned number)
{
    do
    {
        redo_measurement = 0;
        end_of_measurement = 0;
        first = 0;
        for (unsigned i = 0; i < cpus_present; ++i)
            per_cpu(wakeups, i) = 0;
        atomic_set(&sync_var, 0);

        on_each_cpu(per_cpu_func, NULL, 1);

        evaluate_global();
        for (unsigned i = 0; i < cpus_present; ++i)
            evaluate_cpu(i);

        restore_hpet_after_measurement();
    } while (redo_measurement);

    commit_results(number);
}

// Get the unit of the PKG_ENERGY_STATUS MSR in 0.1 microJoule
static inline u32 get_rapl_unit(void)
{
    u64 val;
    rdmsrl_safe(MSR_RAPL_POWER_UNIT, &val);
    val = (val >> 8) & 0b11111;
    return 10000000 / (1 << val);
}

static inline u32 get_cstate_hint(void)
{
    if (target_cstate == 0)
    {
        return 0xf;
    }

    if (target_cstate > 15)
    {
        printk(KERN_ERR "WARNING: target_cstate of %i is invalid, using C1!", target_cstate);
        return 0;
    }

    return target_cstate - 1;
}

// Model and Family calculation as specified in the Intel Software Developer's Manual
static void set_cpu_info(u32 a)
{
    u32 family_id = (a >> 8) & 0xf;
    u32 model_id = (a >> 4) & 0xf;

    cpu_family = family_id;
    if (family_id == 0xf)
    {
        cpu_family += (a >> 20) & 0xff;
    }

    cpu_model = model_id;
    if (family_id == 0x6 || family_id == 0xf)
    {
        cpu_model += ((a >> 16) & 0xf) << 4;
    }
}

static void per_cpu_init(void* info) {
    int err = 0;

    u64 ia32_fixed_ctr_ctrl;
    u64 ia32_perf_global_ctrl;

    err = rdmsrl_safe(IA32_FIXED_CTR_CTRL, &ia32_fixed_ctr_ctrl);
    ia32_fixed_ctr_ctrl |= 0b11 << 8;
    err |= wrmsrl_safe(IA32_FIXED_CTR_CTRL, ia32_fixed_ctr_ctrl);

    err |= rdmsrl_safe(IA32_PERF_GLOBAL_CTRL, &ia32_perf_global_ctrl);
    ia32_perf_global_ctrl |= 1l << 34;
    err |= wrmsrl_safe(IA32_PERF_GLOBAL_CTRL, ia32_perf_global_ctrl);

    if(err) {
        printk(KERN_ERR "WARNING: Could not enable 'unhalted' register.\n");
    }
}

static int mwait_init(void)
{
    int err;

    u32 a = 0x1, b, c, d;
    asm("cpuid;"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "0"(a));
    if (!(c & (1 << 3)))
    {
        printk(KERN_ERR "WARNING: Mwait not supported.\n");
    }
    set_cpu_info(a);
    printk(KERN_INFO "CPU FAMILY: 0x%x, CPU Model: 0x%x\n", cpu_family, cpu_model);

    a = 0x5;
    asm("cpuid;"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "0"(a));
    if (!(c & 1))
    {
        printk(KERN_ERR "WARNING: Mwait Power Management not supported.\n");
    }

    a = 0x80000007;
    asm("cpuid;"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "0"(a));
    if (!(d & (1 << 8)))
    {
        printk(KERN_ERR "WARNING: TSC not invariant, sleepstate statistics potentially meaningless.\n");
    }

    on_each_cpu(per_cpu_init, NULL, 1);

    if(*mwait_hint==NULL) {
        calculated_mwait_hint = 0;
        calculated_mwait_hint += target_subcstate & MWAIT_SUBSTATE_MASK;
        calculated_mwait_hint += (get_cstate_hint() & MWAIT_CSTATE_MASK) << MWAIT_SUBSTATE_SIZE;
    } else {
        if(kstrtou32(mwait_hint, 0, &calculated_mwait_hint)) {
            calculated_mwait_hint = 0;
            printk(KERN_ERR "Interpreting mwait_hint failed, falling back to hint 0x0!\n");
        }
    }
    printk(KERN_INFO "Using MWAIT hint 0x%x.", calculated_mwait_hint);

    measurement_count = measurement_count < MAX_NUMBER_OF_MEASUREMENTS
                            ? measurement_count
                            : MAX_NUMBER_OF_MEASUREMENTS;

    rapl_unit = get_rapl_unit();
    printk(KERN_INFO "rapl_unit in 0.1 microJoule: %u\n", rapl_unit);

    cpus_present = num_present_cpus();
    if (cpus_mwait == -1)
        cpus_mwait = cpus_present;

    register_nmi_handler(NMI_UNKNOWN, nmi_handler, NMI_FLAG_FIRST, "nmi_handler");

    apic_id_of_cpu0 = default_cpu_present_to_apicid(0);

    hpet_period = get_hpet_period();
    hpet_pin = select_hpet_pin();
    if (hpet_pin == -1)
    {
        printk(KERN_ERR "ERROR: No suitable pin found for HPET, aborting!\n");
        return 1;
    }
    printk(KERN_INFO "Using IOAPIC pin %i for HPET.\n", hpet_pin);

    setup_ioapic_for_measurement(apic_id_of_cpu0, hpet_pin);

    for (unsigned i = 0; i < measurement_count; ++i)
    {
        measure(i);
    }

    restore_ioapic_after_measurement();

    err = kobject_init_and_add(&(pkg_stats.kobject), &pkg_ktype, NULL, "mwait_measurements");
    for (unsigned i = 0; i < cpus_present; ++i)
    {
        err |= kobject_init_and_add(&(cpu_stats[i].kobject), &cpu_ktype, &(pkg_stats.kobject), "cpu%u", i);
    }
    if (err)
        printk(KERN_ERR "ERROR: Could not properly initialize CPU stat structure in the sysfs.\n");

    printk(KERN_INFO "MWAIT: Measurements done.\n");

    return 0;
}

static void mwait_exit(void)
{
    for (unsigned i = 0; i < cpus_present; ++i)
    {
        kobject_del(&(cpu_stats[i].kobject));
    }
    kobject_del(&(pkg_stats.kobject));

    unregister_nmi_handler(NMI_UNKNOWN, "nmi_handler");
}

module_init(mwait_init);
module_exit(mwait_exit);
