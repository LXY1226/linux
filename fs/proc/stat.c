#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/irqnr.h>
#include <linux/cputime.h>
#include <linux/tick.h>

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

#ifdef arch_idle_time

static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}

#else

static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = usecs_to_cputime64(idle_time);

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = -1ULL;

	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = usecs_to_cputime64(iowait_time);

	return iowait;
}

#endif

static int show_stat(struct seq_file *p, void *v)
{
	int i, j;
#if defined(MY_ABC_HERE)
//do nothing
#else /* MY_ABC_HERE */
	unsigned long jif;
#endif /* MY_ABC_HERE */
	u64 user, nice, system, idle, iowait, irq, softirq, steal;
	u64 guest, guest_nice;
	u64 sum = 0;
	u64 sum_softirq = 0;
	unsigned int per_softirq_sums[NR_SOFTIRQS] = {0};
#if defined(MY_ABC_HERE)
	struct timespec64 boottime;
#else /* MY_ABC_HERE */
	struct timespec boottime;
#endif /* MY_ABC_HERE */

	user = nice = system = idle = iowait =
		irq = softirq = steal = 0;
	guest = guest_nice = 0;
#if defined(MY_ABC_HERE)
	getboottime64(&boottime);
#else /* MY_ABC_HERE */
	getboottime(&boottime);
	jif = boottime.tv_sec;
#endif /* MY_ABC_HERE */

	for_each_possible_cpu(i) {
		user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle += get_idle_time(i);
		iowait += get_iowait_time(i);
		irq += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal += kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest += kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
		sum += kstat_cpu_irqs_sum(i);
		sum += arch_irq_stat_cpu(i);

		for (j = 0; j < NR_SOFTIRQS; j++) {
			unsigned int softirq_stat = kstat_softirqs_cpu(j, i);

			per_softirq_sums[j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();

	seq_puts(p, "cpu ");
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(user));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(nice));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(system));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(idle));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(iowait));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(irq));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(softirq));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(steal));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(guest));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(guest_nice));
	seq_putc(p, '\n');

	for_each_online_cpu(i) {
		/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
		user = kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice = kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system = kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle = get_idle_time(i);
		iowait = get_iowait_time(i);
		irq = kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq = kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal = kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest = kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice = kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
		seq_printf(p, "cpu%d", i);
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(user));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(nice));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(system));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(idle));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(iowait));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(irq));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(softirq));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(steal));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(guest));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(guest_nice));
		seq_putc(p, '\n');
	}
	seq_printf(p, "intr %llu", (unsigned long long)sum);

	/* sum again ? it could be updated? */
	for_each_irq_nr(j)
		seq_put_decimal_ull(p, ' ', kstat_irqs_usr(j));

	seq_printf(p,
		"\nctxt %llu\n"
#if defined(MY_ABC_HERE)
		"btime %llu\n"
#else /* MY_ABC_HERE */
		"btime %lu\n"
#endif /* MY_ABC_HERE */
		"processes %lu\n"
		"procs_running %lu\n"
		"procs_blocked %lu\n",
		nr_context_switches(),
#if defined(MY_ABC_HERE)
		(unsigned long long)boottime.tv_sec,
#else /* MY_ABC_HERE */
		(unsigned long)jif,
#endif /* MY_ABC_HERE */
		total_forks,
		nr_running(),
		nr_iowait());

	seq_printf(p, "softirq %llu", (unsigned long long)sum_softirq);

	for (i = 0; i < NR_SOFTIRQS; i++)
		seq_put_decimal_ull(p, ' ', per_softirq_sums[i]);
	seq_putc(p, '\n');

	return 0;
}

static int stat_open(struct inode *inode, struct file *file)
{
	size_t size = 1024 + 128 * num_online_cpus();

	/* minimum size to display an interrupt count : 2 bytes */
	size += 2 * nr_irqs;
	return single_open_size(file, show_stat, NULL, size);
}

static const struct file_operations proc_stat_operations = {
	.open		= stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_stat_init(void)
{
	proc_create("stat", 0, NULL, &proc_stat_operations);
	return 0;
}
fs_initcall(proc_stat_init);
