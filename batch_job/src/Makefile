include ../../config.mk
include ../../rules.mk

LIBRARIES = libbatch_job.a

PROGRAMS = work_queue_factory work_queue_pool

ifeq ($(CCTOOLS_CHIRP),chirp)
CHIRP_LIB=../../chirp/src/libchirp.a
CHIRP_BATCH=batch_job_chirp.c
endif

EXTERNAL_LIBRARIES = ../../work_queue/src/libwork_queue.a $(CHIRP_LIB) ../../dttools/src/libdttools.a

SOURCES = \
	batch_file.c \
	batch_task.c \
	batch_wrapper.c \
	batch_job.c \
	batch_job_amazon.c \
	batch_job_dryrun.c \
	$(CHIRP_BATCH) \
	batch_job_cluster.c \
	batch_job_blue_waters.c \
	batch_job_condor.c \
	batch_job_local.c \
	batch_job_work_queue.c\
	batch_job_mesos.c \
	mesos_task.c

PUBLIC_HEADERS = batch_job.h

OBJECTS = $(SOURCES:%.c=%.o)

all: $(LIBRARIES) $(PROGRAMS)

libbatch_job.a: $(OBJECTS)

work_queue_factory: work_queue_factory.o libbatch_job.a $(EXTERNAL_LIBRARIES)

# Note that work_queue_pool is the same as work_queue_factory, for backwards compatibility.
work_queue_pool: work_queue_factory
	cp $< $@

install:
	mkdir -p $(CCTOOLS_INSTALL_DIR)/bin
	cp $(PROGRAMS) $(CCTOOLS_INSTALL_DIR)/bin
	mkdir -p $(CCTOOLS_INSTALL_DIR)/lib
	cp $(LIBRARIES) $(CCTOOLS_INSTALL_DIR)/lib
	mkdir -p $(CCTOOLS_INSTALL_DIR)/include/cctools
	cp $(PUBLIC_HEADERS) $(CCTOOLS_INSTALL_DIR)/include/cctools

clean:
	rm -rf $(OBJECTS) $(LIBRARIES) $(PROGRAMS) batch_job_amazon_script.c *.o

