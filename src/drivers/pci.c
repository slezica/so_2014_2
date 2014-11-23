#include <kernel.h>
#include <lib.h>

// PCI I/O ports:
#define IO_ADDRESS 0xCF8
#define IO_DATA    0xCFC


// Static data:
static unsigned    devcount = 0;
static PCIDevice_t devices[PCI_MAX_DEVICES] = {};


/*
    Hardware PCI headers (order and alignment critical):
*/
struct pci_common {
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

} __attribute__((packed));


struct pci_generic {
    struct pci_common common;
    char   padding[48];

} __attribute__((packed));


struct pci_device {
    struct pci_common common;

    int   address[6];
    int   cis_ptr;
    short subsys_vendor;
    short subsys;
    int   rom_base;
    char  capabilities;
    char  reserved[7];
    char  irq;
    char  ipin;
    char  min_grant;
    char  max_latency;

    char  padding[8];

} __attribute__((packed));


struct pci_pci_bridge {
    struct pci_common common;

    int   address[2];
    char  bus;
    char  bus2;
    char  bus3;
    char  latency2;
    char  io_base;
    char  io_limit;
    short status2;
    short mem_base;
    short mem_limit;
    int   pref_base;
    int   pref_limit;
    char  capabilities;
    char  reserved[3];
    int   rom_base;
    char  irq;
    char  ipin;
    short bridge_ctl;

    char  padding[8];

} __attribute__((packed));


struct pci_card_bridge {
    struct pci_common common;

    int   card_base;
    char  capabilities;
    char  reserved;
    short status2;
    char  pci_bus;
    char  card_bus;
    char  sub_bus;
    char  card_latency;
    int   base0;
    int   limit0;
    int   base1;
    int   limit1;
    int   io_base0;
    int   io_limit0;
    int   io_base1;
    int   io_limit1;
    char  irq;
    char  ipin;
    short bridge_ctl;
    short sub_device;
    short sub_vendor;
    int   card_legacy_base;

} __attribute__((packed));



/*
    DEVICE TABLE

    The device table is made of entries in the following format:
*/
struct vnames {
    unsigned  id;
    char     *vendor;
    char     *device;
};
/*
    The entry ID is a 32-bit integer, where the high 16 bits are the vendor ID
    and the lower 16 bits are the device ID.:

        id = (vendor << 16) | device

    There's an extra entry for each vendor, with a device ID of 0 and device
    name set to NULL, so that vendors can be found independently.
*/


/*
    CLASS TABLE

    Like the device table, but the ID is created from the class and subclass
    8-bit integers, leaving a short integer where the lower 8 bits are the
    subclass, and bits 8-16 are the class.
*/
struct cnames {
    unsigned  id;
    char     *class;
    char     *subclass;
};


// Actual static tables:
struct vnames vnames_table[] = {
    #include <pci/devices.table>
};

struct cnames cnames_table[] = {
    #include <pci/classes.table>
};


/*
    FINDING VENDOR, DEVICE, CLASS AND SUBCLASS NAMES

    They can be found in pairs, using:
        find_vnames(vendor, device)
        find_cnames(class, subclass)
*/
static struct vnames *find_vnames(unsigned short vendor, unsigned short device) {
    unsigned id = (vendor << 16) | device;

    unsigned i, entries = sizeof(vnames_table) / sizeof(struct vnames);

    for (i = 0; i < entries; i++)
        if (vnames_table[i].id == id)
            return &(vnames_table[i]);

    return NULL;
}

static struct cnames *find_cnames(unsigned char class, unsigned char subclass) {
    unsigned id = (class << 8) | subclass;

    unsigned i, entries = sizeof(cnames_table) / sizeof(struct cnames);

    for (i = 0; i < entries; i++)
        if (cnames_table[i].id == id)
            return &(cnames_table[i]);

    return NULL;
}



// Hardware PCI register reading:
static int in_reg(char bus, char number, char function, char reg) {
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


static void in_header(char bus, char number, char function, struct pci_generic *out) {
    int *registers = (int *) out;

    int i;
    for (i = 0; i < sizeof(struct pci_generic) / 4; i++)
        registers[i] = in_reg(bus, number, function, i);
}


static void read_device(struct pci_device *in, PCIFunction_t *out) {
    out->irq  = in->irq;
    out->ipin = in->ipin;

    int i;
    for (i = 0; i < 6; i++)
        out->as.device.address[i] = in->address[i] & (~0xF);
}


static void read_pci_bridge(struct pci_pci_bridge *in, PCIFunction_t *out) {
    out->irq  = in->irq;
    out->ipin = in->ipin;

    int i;
    for (i = 0; i < 2; i++)
        out->as.device.address[i] = in->address[i] & (~0xF);

    out->as.pci_bridge.io_base    = in->io_base;
    out->as.pci_bridge.io_limit   = in->io_limit;
    out->as.pci_bridge.pref_base  = in->pref_base;
    out->as.pci_bridge.pref_limit = in->pref_limit;
}


static void read_card_bridge(struct pci_card_bridge *in, PCIFunction_t *out) {
    out->irq  = in->irq;
    out->ipin = in->ipin;

    out->as.card_bridge.address[0] = in->base0;
    out->as.card_bridge.limit[0]   = in->limit0;
    out->as.card_bridge.address[1] = in->base1;
    out->as.card_bridge.limit[1]   = in->limit1;

    out->as.card_bridge.io_address[0] = in->io_base0;
    out->as.card_bridge.io_limit[0]   = in->io_limit0;
    out->as.card_bridge.io_address[1] = in->io_base1;
    out->as.card_bridge.io_limit[1]   = in->io_limit1;
}


static void read_generic(struct pci_generic *in, PCIFunction_t *out) {
    out->type = in->common.type;

    out->vendor_id   = in->common.vendor;
    out->device_id   = in->common.device;
    out->revision    = in->common.revision;
    out->class_id    = in->common.class;
    out->subclass_id = in->common.subclass;

    struct vnames *vnames = find_vnames(in->common.vendor, in->common.device);
    struct cnames *cnames = find_cnames(in->common.class, in->common.subclass);

    strncpy(out->vendor  , vnames->vendor  , PCI_MAX_NAME_LEN);
    strncpy(out->device  , vnames->device  , PCI_MAX_NAME_LEN);
    strncpy(out->class   , cnames->class   , PCI_MAX_NAME_LEN);
    strncpy(out->subclass, cnames->subclass, PCI_MAX_NAME_LEN);

    switch (in->common.type) {
        case PCI_TYPE_DEVICE:
            read_device((struct pci_device*) in, out);
        break;
        case PCI_TYPE_PCI_BRIDGE:
            read_pci_bridge((struct pci_pci_bridge*) in, out);
        break;
        case PCI_TYPE_CARD_BRIDGE:
            read_card_bridge((struct pci_card_bridge*) in, out);
        break;
    }
}


static bool scan(char bus, char number, PCIDevice_t *out) {
    if ((short) in_reg(bus, number, 0, 0) == -1)
        return false; // No device on bus:number

    out->bus    = bus;
    out->number = number;
    out->fcount = 0;

    int i;
    for (i = 0; i < 8; i++) {
        struct pci_generic header;
        memset(&header, 0, sizeof(header));
        in_header(bus, number, i, &header);

        if (header.common.vendor == -1)
            continue;

        PCIFunction_t *f = &(out->functions[ out->fcount++ ]);
        read_generic(&header, f);
    }

    return true;
}

static void scanAll() {
    int bus, number;
    PCIDevice_t device;

    for (bus = 0; bus < 256; bus++)
    for (number = 0; number < 32; number++) {
        bool found = scan(bus, number, &device);

        if (found)
            devices[devcount++] = device;
    }
}


void mt_pci_init() {
    scanAll();
}


bool mt_pci_info(unsigned devnum, PCIDevice_t *out) {
    if (devnum >= devcount)
        return false;

    memcpy(out, &devices[devnum], sizeof(PCIDevice_t));

    return true;
}