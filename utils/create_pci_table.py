#-*- coding: utf-8 -*-

import sys, os
from collections import namedtuple


Vendor    = namedtuple('Vendor'   , ['code', 'name', 'devices'   ])
Device    = namedtuple('Device'   , ['code', 'name', 'subdevices'])
Subdevice = namedtuple('Subdevice', ['subvcode', 'code', 'name'  ])
Class     = namedtuple('Class'    , ['code', 'name', 'subclasses']) # Recursive

vendors = []
classes = []


def sanitize(text):
    return text.replace(r'"', r'\"')


def process_device_line(line):
    tabs = len(line) - len(line.lstrip())
    line = line.strip()

    if len(line) == 0 or line.startswith('#'):
        return

    if tabs == 0:
        code, name = line.split(None, 1)
        vendors.append( Vendor(int(code, 16), sanitize(name), []) )

    elif tabs == 1:
        code, name = line.split(None, 1)
        vendors[-1].devices.append( Device(int(code, 16), sanitize(name), []) )

    elif tabs == 2:
        parts = line.split(None, 2)
        if len(parts) == 2: parts.append('(No name)')

        subdevice = Subdevice(*parts)
        vendors[-1].devices[-1].subdevices.append(subdevice)


def generate_device_table():
    for vendor in vendors:
        print '{ %s, "%s", NULL },' % (hex(vendor.code << 16), vendor.name)

        for device in vendor.devices:
            print '{ %s, "%s", "%s" },' % (
                hex(vendor.code << 16 | device.code),
                vendor.name,
                device.name
            )


def process_class_line(line):
    tabs = len(line) - len(line.lstrip())
    line = line.strip()

    if len(line) == 0 or line.startswith('#'):
        return

    code, name = line.split(None, 1)
    cls = Class(int(code, 16), sanitize(name), [])

    if   tabs == 0: classes.append(cls)
    elif tabs == 1: classes[-1].subclasses.append(cls)
    elif tabs == 2: classes[-1].subclasses[-1].subclasses.append(cls)


def generate_class_table():
    fmt = '{ %s, "%s", "%s" },'

    for cls in classes:
        for subcls in cls.subclasses:
            code = (cls.code << 8) | subcls.code
            print fmt % (hex(code), cls.name, subcls.name)

            # for subsubcls in subcls.subclasses:
            #     code = cls.code << 16 | subcls.code << 8 | subsubcls.code
            #     print '{ %s, "%s" },' % (hex(code), subsubcls.name)

        # Special entry for unspecified subclass (0xFF):
        code = cls.code << 8 | 0xFF
        print '{ %s, "%s", "%s" },' % (hex(code), cls.name, subcls.name)




def main():
    if len(sys.argv) < 2:
        print "python create_pci_table.py [devices|classes]"
        return

    table = sys.argv[1]

    if table == 'devices':
        path     = os.path.dirname(__file__) + "/pci_devices.txt"
        process  = process_device_line
        generate = generate_device_table

    elif table == 'classes':
        path     = os.path.dirname(__file__) + "/pci_classes.txt"
        process  = process_class_line
        generate = generate_class_table

    else:
        print "create_pci_table [devices|classes]"
        return

    print "// THIS TABLE WAS AUTOMATICALLY GENERATED. DO NOT EDIT"
    print "// (utils/create_pci_table.py)"

    with open(path):
        line = None

        try:
            for line in open(path):
                process(line)

        except Exception as e:
            print 'ERROR PROCESSING "%s"' % line.rstrip()
            raise

    generate()

main()