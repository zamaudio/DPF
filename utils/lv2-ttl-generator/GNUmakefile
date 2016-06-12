#!/usr/bin/makefile -f

all: build

ifeq ($(WIN32),true)
build: ../lv2_ttl_generator32.exe
else
 ifeq ($(WIN64),true)
build: ../lv2_ttl_generator64.exe
 else
  ifeq ($(MACOS),true)
build: ../lv2_ttl_generatorosx
  else
build: ../lv2_ttl_generator
  endif
 endif
endif

../lv2_ttl_generator: lv2_ttl_generator.c
	$(CC) $< $(CFLAGS) -o $@ $(LDFLAGS) -ldl

../lv2_ttl_generatorosx: lv2_ttl_generator.c
	$(CC) $< $(CFLAGS) -o $@ $(LDFLAGS) -ldl
	touch ../lv2_ttl_generator

../lv2_ttl_generator32.exe: lv2_ttl_generator.c
	$(CC) $< -m32 $(CFLAGS) -o $@ $(LDFLAGS) -static
	touch ../lv2_ttl_generator

../lv2_ttl_generator64.exe: lv2_ttl_generator.c
	$(CC) $< $(CFLAGS) -o $@ $(LDFLAGS) -static
	touch ../lv2_ttl_generator

clean:
	rm -f ../lv2_ttl_generator ../lv2_ttl_generator32.exe ../lv2_ttl_generator64.exe ../lv2_ttl_generatorosx
