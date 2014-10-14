
CFLAGS := -O3 -g -Wall -Werror -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE

OBJECTS := fair-lio.o 

LDFLAGS :=
LIBS := -laio -lrt

depfile = $(@D)/.$(@F).dep
tmpdepfile = $(@D)/.$(@F).dep.tmp
DEPFLAG = -Wp,-MD,$(tmpdepfile)

dep_files := $(wildcard $(foreach f, $(OBJECTS),$(dir $(f)).$(notdir $(f)).dep))
tmp_depfiles := $(wildcard $(addsuffix .tmp,$(dep_files)))

.PHONY: all
all: real_all

ifneq ($(dep_files),)
  include $(dep_files)
endif

real_all: fair-lio

fair-lio: $(OBJECTS)
	$(CC) -o $@ $+ $(LDFLAGS) $(LIBS)

clean:
	rm -f fair-lio $(OBJECTS) $(dep_files)
	rm -f $(tmp_depfiles)

.c.o:
	$(CC) $(CFLAGS) $(CFLAGS_$(@F)) $(DEPFLAG) -c -o $@ $<
	sed -e 's~^.*:~$@:~' $(tmpdepfile) > $(depfile)
	rm -f $(tmpdepfile)

.c.s:
	$(CC) $(CFLAGS) $(CFLAGS_$(@F)) $(DEPFLAG) -S -o $@ $<
	sed -e 's~^.*:~$@:~' $(tmpdepfile) > $(depfile)
	rm -f $(tmpdepfile)

