#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OS Student");
MODULE_DESCRIPTION("Kprobe kernel_clone tracer");
MODULE_VERSION("1.0");

static struct kprobe kp = {
 .symbol_name = "_do_fork",
};

// Функция перехвата, выполняющаяся ПЕРЕД вызовом kernel_clone()
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    // Печать в dmesg лог ядра: PID, TGID (пользовательский PID процессов) и имя бинарника (comm)
    printk(KERN_INFO "clone_probe: [DETECTED] pid=%d tgid=%d comm=%s\n",
           current->pid, current->tgid, current->comm);
    return 0;
}

static int __init probe_init(void)
{
    kp.pre_handler = handler_pre;
    int ret = register_kprobe(&kp);
    if (ret < 0) {
        printk(KERN_ERR "clone_probe: register_kprobe failed, returned %d\n", ret);
        return ret;
    }
    printk(KERN_INFO "clone_probe: kprobe registered successfully on kernel_clone\n");
    return 0;
}

static void __exit probe_exit(void)
{
    unregister_kprobe(&kp);
    printk(KERN_INFO "clone_probe: kprobe unregistered\n");
}

module_init(probe_init);
module_exit(probe_exit);
