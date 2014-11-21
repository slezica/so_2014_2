#include <kernel.h>
#include <lib.h>

// PCI I/O ports:
#define IO_ADDRESS 0xCF8
#define IO_DATA    0xCFC


// Static data:
static unsigned    devcount = 0;
static PCIDevice_t devices[PCI_MAX_DEVICES] = {};


/*
    TABLA DE DISPOSITIVOS

    La tabla de dispositivos está compuesta por entradas en formato:
        vdcode: los 16 bits altos son el vendor_id, los bajos el device_id
        vname : nombre del vendor
        dname : nombre del dispositivo

    Por cada vendor, hay una entrada especial cuyo índice tiene device_id 0,
    y cuyo dname es NULL. Esto permite encontrar vendors independientemente.

    Está almacenada en formato estático en include/ipc/devices.table, y se la
    incluye acá.

*/


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


static bool scan(char bus, char number, PCIDevice_t *out) {
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

        PCIFunction_t *f = &(out->functions[ out->fcount++ ]);

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