#include <kernel.h>


void printFunction(PCIFunction_t *f) {
    printk("  Vendor: %s\n", f->vendor);
    printk("  Device: %s (rev %.2x)\n", f->device, f->revision);
    printk("  Class : %s\n", f->class);
    printk("  Subcls: %s\n", f->subclass);

    if (f->irq != 0)
        printk("  IRQ   : %u (pin %u)\n", f->irq, f->ipin);
}


void printDevice(unsigned index, PCIDevice_t *device) {
    printk("PCI %u (bus %u:%u)", index, device->bus, device->number);

    if (device->fcount > 1)
        printk(" (%d functions)", device->fcount);

    printk("\n");

    unsigned i;
    for (i = 0; i < device->fcount; i++) {
        printFunction(&(device->functions[i]));
        printk("\n");
    }
}


void printDevices() {
    unsigned    i;
    PCIDevice_t device;

    for (i = 0; i < PCI_MAX_DEVICES; i++) {
        if (! mt_pci_info(i, &device))
            break; // No more devices

        printDevice(i, &device);
    }
}



int lspci_main(int argc, char *argv[]) {
    printDevices();
    return 0;
}
