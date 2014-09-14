CPPFLAGS = -Isys

BINS = sunglasses

all: libs $(BINS)

sunglasses:     blurred.o neons.o floor.o highway.o metaballs.o plotter.o \
                intro.o punk.o shapes.o theme.o uvmap.o uvmap-smc.o \
                theme-neons.o farted.o endscroll.o startup.o libp61.a libsys.a

SUBDIRS = ahx p61 sys

libs:
	@for subdir in $(SUBDIRS); do $(MAKE) -C $$subdir || exit 1; done

clean-libs:
	@for subdir in $(SUBDIRS); do $(MAKE) -C $$subdir clean; done

clean::	clean-libs
	rm -vf sintab.s
	rm -f $(BINS) *.zip

archive:
	7z a "a500-$$(date +%F-%H%M).zip" $(BINS) data/*.{ilbm,2d,tga,bin,p61,smp,sync,txt}

floppy:
	-rm -rf floppy
	mkdir -p floppy/data
	cp -a $(BINS) floppy/
	cp -a data/*.{ilbm,2d,tga,bin,p61,smp,sync,txt} floppy/data/

include Makefile.common

.PHONY:	floppy libs clean-libs

# vim: ts=8 sw=8 expandtab
