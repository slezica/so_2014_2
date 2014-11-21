#include <kernel.h>
#include <lib.h>


#define PCI_MAX_DEVICES 16
#define PCI_MAX_NAME_LEN 128


// PCI I/O ports:
#define IO_ADDRESS 0xCF8
#define IO_DATA    0xCFC


struct vnames {
    unsigned  id;
    char     *vendor;
    char     *device;
};

struct vnames vnames_table[] = {
    #include <pci/devices.table>
};

static struct vnames *find_vnames(unsigned short vendor, unsigned short device) {
    unsigned id = (vendor << 16) | device;

    unsigned i, entries = sizeof(vnames_table) / sizeof(struct vnames);

    for (i = 0; i < entries; i++)
        if (vnames_table[i].id == id)
            return &(vnames_table[i]);

    return NULL;
}


struct cnames {
    unsigned  id;
    char     *class;
    char     *subclass;
};

struct cnames cnames_table[] = {
    #include <pci/classes.table>
};

static struct cnames *find_cnames(unsigned char class, unsigned char subclass) {
    unsigned id = (class << 8) | subclass;

    unsigned i, entries = sizeof(cnames_table) / sizeof(struct cnames);

    for (i = 0; i < entries; i++)
        if (cnames_table[i].id == id)
            return &(cnames_table[i]);

    return NULL;
}


struct device_header {
    short vendor;
    short device;
    short command;
    short status;
    char  revision;
    char  prog_if;
    char  subclass;
    char  class;
    char  cache_ls;
    char  latency;
    char  type;
    char  bist;
    int   addresses[6];
    int   cis_ptr;
    short subsys_vendor;
    short subsys;
    int   rom_base;
    char  capabilities_ptr;
    char  reserved[7];
    char  irq;
    char  ipin;
    char  min_grant;
    char  max_latency;

} __attribute__((packed));


static int read_reg(char bus, char number, char function, char reg) {
    // Structure of a PCI configuration address:
    // 31      30-24     23-16  15-11   10-8      7-0
    // Enable  Reserved  Bus    Device  Function  Register

    int lregister = reg       * 4; // Turn into offset in bytes
    int lfunction = function << 8;
    int lnumber   = number   << 11;
    int lbus      = bus      << 16;
    int lenable   = 1        << 31;

    int address = lenable | lbus | lnumber | lfunction | lregister;

    outl(IO_ADDRESS, address);
    return inl(IO_DATA);
}


static void read_header(char bus, char number, char function, struct device_header *out) {
    int *registers = (int *) out;

    int i;
    for (i = 0; i < 16; i++)
        registers[i] = read_reg(bus, number, function, i);
}


typedef struct {
    short  vendor_id;
    char   vendor[PCI_MAX_NAME_LEN];

    short  device_id;
    char   device[PCI_MAX_NAME_LEN];

    char  class_id;
    char  class[PCI_MAX_NAME_LEN];

    char  subclass_id;
    char  subclass[PCI_MAX_NAME_LEN];

    char revision;
    char irq, ipin;
} PCIFunction;

typedef struct {
    char        bus, number;
    unsigned    fcount;
    PCIFunction functions[8];
} PCIDevice;


static unsigned  devcount = 0;
static PCIDevice devices[PCI_MAX_DEVICES] = {};


static bool scan(char bus, char number, PCIDevice *out) {
    if ((short) read_reg(bus, number, 0, 0) == -1)
        return false; // No device on bus:number

    out->bus    = bus;
    out->number = number;
    out->fcount = 0;

    int i;
    for (i = 0; i < 8; i++) {

        struct device_header header;
        read_header(bus, number, i, &header);

        if (header.vendor == -1)
            continue;

        PCIFunction *f = &(out->functions[ out->fcount++ ]);

        struct vnames *vnames = find_vnames(header.vendor, header.device);
        struct cnames *cnames = find_cnames(header.class, header.subclass);

        f->vendor_id   = header.vendor;
        f->device_id   = header.device;
        f->class_id    = header.class;
        f->subclass_id = header.subclass;

        strncpy(f->vendor  , vnames->vendor  , PCI_MAX_NAME_LEN);
        strncpy(f->device  , vnames->device  , PCI_MAX_NAME_LEN);
        strncpy(f->class   , cnames->class   , PCI_MAX_NAME_LEN);
        strncpy(f->subclass, cnames->subclass, PCI_MAX_NAME_LEN);
    }

    return true;
}

void scanAll() {
    int bus, number;
    PCIDevice device;

    for (bus = 0; bus < 256; bus++)
    for (number = 0; number < 32; number++) {
        bool found = scan(bus, number, &device);

        if (found)
            devices[devcount++] = device;
    }
}


void printFunction(PCIFunction *f) {
    printk("  Vendor: %s\n", f->vendor);
    printk("  Device: %s (rev %.2x)\n", f->device, f->revision);
    printk("  Class : %s\n", f->class);
    printk("  Subcls: %s\n", f->subclass);

    if (f->irq != 0)
        printk("  IRQ   : %u (pin %u)\n", f->irq, f->ipin);
}


void printDevice(unsigned index, PCIDevice *device) {
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
    unsigned i;
    for (i = 1; i < 2; i++)
        printDevice(i, &devices[i]);
}



int lspci_main(int argc, char *argv[]) {
    scanAll();
    printDevices();
    return 0;
}
