// #include <kernel.h>
// #include <integer.h>


// #define CONFIG_ADDRESS 0xCF8
// #define CONFIG_DATA    0xCFC
// #define NO_DEVICE      0xFFFF


// /*
//     TABLA DE DISPOSITIVOS

//     La tabla de dispositivos está compuesta por entradas en formato:
//         vdcode: los 16 bits altos son el vendor_id, los bajos el device_id
//         vname : nombre del vendor
//         dname : nombre del dispositivo

//     Por cada vendor, hay una entrada especial cuyo índice tiene device_id 0,
//     y cuyo dname es NULL. Esto permite encontrar vendors independientemente.

//     Está almacenada en formato estático en include/ipc/devices.table, y se la
//     incluye acá.

// */

// struct device_entry {
//     uint32_t  id;
//     char     *vendor;
//     char     *device;
// };

// struct device_entry device_table[] = {
//     #include <pci/devices.table>
// };


// struct class_entry {
//     uint32_t  id;
//     char     *class;
//     char     *subclass;
// };


// struct class_entry class_table[] = {
//     #include <pci/classes.table>
// };


// // void pci_table_find(int32_t id);


// void pci_find_vd_names(uint16_t vid, char **vout, uint16_t did, char **dout) {
//     // Arguments:
//     //   vid : vendor ID
//     //   vout: vendor name output parameter (writes a char*)
//     //   did : device ID
//     //   dout: device name output parameter (writes a char*)
//     size_t   i;
//     uint32_t id = (vid << 16) | did;

//     for (i = 0; i < sizeof(device_table) / sizeof(struct device_entry); i++) {

//         if (device_table[i].id == id) {
//             *vout = device_table[i].vendor;
//             *dout = device_table[i].device;
//         }
//     }
// }


// void pci_find_cls_names(uint16_t cid, char **out, uint16_t scid, char **subout) {
//     // Arguments:
//     //   cid   : class ID
//     //   out   : class name output parameter (writes a char*)
//     //   scid  : subclass ID
//     //   subout: subclass name output parameter (writes a char*)
//     size_t   i;
//     uint32_t id = (cid << 16) | (scid << 8);

//     for (i = 0; i < sizeof(class_table) / sizeof(struct class_entry); i++) {

//         if (class_table[i].id == id) {
//             *out    = class_table[i].class;
//             *subout = class_table[i].subclass;
//         }
//     }
// }


// // Funciones de acceso a hardware:

// static uint32_t pci_read32(uint8_t bus, uint8_t number, uint8_t function, uint8_t offset) {
//     uint32_t address;
//     // Estructura de address:
//     // 31      30-24     23-16  15-11   10-8      7-0
//     // Enable  Reserved  Bus    Device  Function  Register

//     uint32_t lenable   = 1 << 31;
//     uint32_t lbus      = bus << 16;
//     uint32_t lnumber   = number << 11;
//     uint32_t lfunction = function << 8;
//     uint32_t loffset   = offset & 0xFC; // los accesos son alineados

//     address = lenable | lbus | lnumber | lfunction | loffset;

//     outl(CONFIG_ADDRESS, address);
//     uint32_t data = inl(CONFIG_DATA);

//     return data;
// }

// static uint16_t pci_read16(uint8_t bus, uint8_t number, uint8_t function, uint8_t offset) {
//     uint32_t data = pci_read32(bus, number, function, offset);

//     // Hay que shiftear y maskear para compensar el alineamiento a 32 bits
//     return data >> ((offset & 2) * 8) & 0xFFFF;
// }

// static uint8_t pci_read8(uint8_t bus, uint8_t number, uint8_t function, uint8_t offset) {
//     uint32_t data = pci_read32(bus, number, function, offset);

//     // Hay que shiftear y maskear para compensar el alineamiento a 32 bits
//     return data >> ((offset & 3) * 8) & 0xFF;
// }


// bool mt_pci_scan(uint8_t bus, uint8_t number, PCIDevice *out) {
//     uint16_t vendor_id = pci_read16(bus, number, 0, 0);

//     if (vendor_id == NO_DEVICE)
//         return false;

//     out->bus    = bus;
//     out->number = number;

//     uint8_t i, function_id = 0;

//     for (i = 0; i < 8; i++) {
//         vendor_id = pci_read16(bus, number, i, 0);

//         if (vendor_id == NO_DEVICE)
//             continue;

//         PCIFunction *f = &(out->functions[ function_id++ ]);

//         f->vendor_id   = vendor_id;
//         f->device_id   = pci_read16(bus, number, i, 0x2);
//         f->class_id    = pci_read8(bus, number, i, 0xB);
//         f->subclass_id = pci_read8(bus, number, i, 0xA);

//         f->revision = pci_read8(bus, number, i, 0x8);
//         f->irq      = pci_read8(bus, number, i, 0x3C);
//         f->ipin     = pci_read8(bus, number, i, 0x3D);

//         pci_find_vd_names(
//             f->vendor_id, &(f->vendor),
//             f->device_id, &(f->device)
//         );

//         pci_find_cls_names(
//             f->class_id   , &(f->class),
//             f->subclass_id, &(f->subclass)
//         );
//     }

//     out->fcount = function_id;
//     return true;
// }
