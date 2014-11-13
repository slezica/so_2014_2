# Uso:
#
# 1) make
#    make mtask
#    Construye el ejecutable y la imagen de CD.
#
# 2) make clean
#    Borra mtask, los objetos y las dependencias.
#
# 3) make new
#    Idem anterior, adicionalmente actualiza fecha y hora de fuentes y headers.

# Directorios de fuentes y headers

SOURCEDIRS = $(shell find src -type d)
INCLUDEDIRS = $(shell find include -type d)

# Archivos fuente y módulos - kstart tiene que ser el primero

SOURCES = $(shell find src -type f -name kstart.S) $(shell find src -type f -name '[a-zA-Z]*.[cS]' -not -name kstart.S)
SOURCENAMES = $(notdir $(SOURCES))
MODULES = $(basename $(SOURCENAMES))

# Headers

HEADERS = $(shell find include -type f)

# Flags de compilación

INCLUDEFLAGS = $(foreach d, $(INCLUDEDIRS), -I $(d))
CFLAGS = -Wall -fno-stack-protector -fno-builtin -m32 $(INCLUDEFLAGS)

# Generar ejecutable (mtask) e imagen de CD (mtask.iso)

OBJECTS = $(MODULES:%=obj/%.o)
mtask: $(OBJECTS)
	@echo "LINK\t" mtask
	@cc -nostdlib -m32 -Wl,-Ttext-segment,0x100000,-Map,mtask.map -o mtask $(OBJECTS)
	@echo "GEN\t" mtask.iso
	@mkdir -p iso/boot/grub
	@cp mtask iso/boot/
	@cp boot/stage2_eltorito boot/menu.lst iso/boot/grub/
	@genisoimage -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o mtask.iso iso 2>/dev/null

# Limpiar

.PHONY: clean
clean:
	@echo CLEAN
	@rm -f obj/* dep/* s/* mtask mtask.map mtask.iso
	@rm -rf iso

# Limpiar y sincronizar fuentes y headers con la fecha y hora de la PC

.PHONY: new
new: clean
	@echo TOUCH
	@touch $(SOURCES) $(HEADERS)

# Path de búsqueda de los fuentes

vpath %.c $(SOURCEDIRS)
vpath %.S $(SOURCEDIRS)

# Generar objetos

obj/%.o: %.c
	@echo "CC\t" $@
	@cc $(CFLAGS) -c $< -o $@

obj/%.o: %.S
	@echo "CC\t" $@
	@cc $(CFLAGS) -c $< -o $@

# Generar salida en assembler

s/%.s: %.c
	@echo "CC\t" $@
	@cc $(CFLAGS) -S $< -o $@

s/%.s: %.S
	@echo "CC\t" $@
	@cc $(CFLAGS) -S $< >$@

# Generar dependencias

dep/%.d: %.c
	@echo "CC\t" $@
	@cc $(CFLAGS) $< -MM -MT 'obj/$*.o $@' > $@

dep/%.d: %.S
	@echo "CC\t" $@
	@cc $(CFLAGS) $< -MM -MT 'obj/$*.o $@' > $@

# Incluir dependencias

DEPS = $(MODULES:%=dep/%.d)
-include $(DEPS)
