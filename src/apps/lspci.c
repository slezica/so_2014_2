#include <kernel.h>


void printDeviceExtras(PCIFunction_t *f) {
    int i;
    for (i = 0; i < 6; i++) {
        int address = f->as.device.address[i];

        if (address)
            printk("  Memory: [%d] at %.8x\n", i, address);
    }
}


void printPciBridgeExtras(PCIFunction_t *f) {
    int i;
    for (i = 0; i < 2; i++) {
        int address = f->as.pci_bridge.address[i];

        if (address)
            printk("  Memory: [%d] at %.8x\n", i, address);
    }

    printk("  I/O   : %.4x to %.4d", f->as.pci_bridge.io_base, f->as.pci_bridge.io_limit);
    printk("  Memory: [%d] at %.8x to %.8x (prefecthable)", f->as.pci_bridge.pref_base << 4, f->as.pci_bridge.pref_limit << 4);
}


void printCardBridgeExtras(PCIFunction_t *f) {
    int i, address, limit;

    for (i = 0; i < 2; i++) {
        address = f->as.card_bridge.address[i];
        limit   = f->as.card_bridge.limit[i];

        if (address)
            printk("  Memory: [%d] at %.8x to %.8x\n", i, address, limit);
    }

    for (i = 0; i < 2; i++) {
        address = f->as.card_bridge.io_address[i];
        limit   = f->as.card_bridge.io_limit[i];

        if (address)
            printk("  I/O   : [%d] at %.8x to %.8x\n", i, address, limit);
    }
}


void printFunction(PCIFunction_t *f) {
    printk("  Vendor: %s\n", f->vendor);
    printk("  Device: %s (rev %d)\n", f->device, f->revision);
    printk("  Class : %s\n", f->class);
    printk("  Subcls: %s\n", f->subclass);

    switch (f->type) {
        case PCI_TYPE_DEVICE:
            printDeviceExtras(f);
        break;

        case PCI_TYPE_PCI_BRIDGE:
            printPciBridgeExtras(f);

        break;
        case PCI_TYPE_CARD_BRIDGE:
            printCardBridgeExtras(f);
        break;
    }

    if (f->irq != 0)
        printk("  IRQ   : %u (pin %u)\n", f->irq, f->ipin);
}


void printPCI(unsigned index, PCIDevice_t *device) {
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


void printAll() {
    PCIDevice_t device;

    int i;
    for (i = 0; i < PCI_MAX_DEVICES; i++) {
        if (! mt_pci_info(i, &device))
            break; // No more devices

        printPCI(i, &device);
    }
}



int lspci_main(int argc, char *argv[]) {
    printAll();
    return 0;
}
