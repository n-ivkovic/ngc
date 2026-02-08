.POSIX:

SHELL = /bin/sh

# Project artefact variables
BASENAME   = ngc
ASMNAME    = asm
EMUNAME    = emu
ALLNAME    = $(ASMNAME) $(EMUNAME)
ASMBIN     = $(BASENAME)-$(ASMNAME)
EMUBIN     = $(BASENAME)-$(EMUNAME)
ALLBIN     = $(ASMBIN) $(EMUBIN)
ASMSRCDIR  = $(ASMNAME)
EMUSRCDIR  = $(EMUNAME)
ASMOBJS    = print.o $(ASMSRCDIR)/llist.o $(ASMSRCDIR)/str.o $(ASMSRCDIR)/err.o $(ASMSRCDIR)/parsed.o $(ASMSRCDIR)/parse.o $(ASMSRCDIR)/assemble.o $(ASMSRCDIR)/assemble_basic.o $(ASMSRCDIR)/assemble_full.o $(ASMSRCDIR)/cli.o
EMUOBJS    = print.o $(EMUSRCDIR)/emu.o $(EMUSRCDIR)/tui.o
ASMMANS    =
EMUMANS    =
ASMINSTALL = $(DESTBINDIR)/$(ASMBIN) $(ASMMANS:%=$(DESTMANDIR)/%)
EMUINSTALL = $(DESTBINDIR)/$(EMUBIN) $(EMUMANS:%=$(DESTMANDIR)/%)

# Project dir variables
SRCDIR  = src
OBJDIR  = obj
BINDIR  = .
MANDIR  = man
TESTDIR = test

ALLSRCDIR = $(SRCDIR) $(ALLBIN:%=$(SRCDIR)/%)
vpath %.c $(ALLSRCDIR)
vpath %.h $(ALLSRCDIR)
vpath %.o $(ALLSRCDIR:$(SRCDIR)/%=$(OBJDIR)/%)

# Compiler variables
CC        = gcc
CPPFLAGS  = #-MMD -MP
CFLAGS    = -std=c99 -Wall -Wextra -Wpedantic
LDFLAGS   =

ifdef DEBUG
	CFLAGS += -Og -g -DDEBUG
else
	CFLAGS += -O3
endif

# Install variables
PREFIX     = /usr/local
DESTBINDIR = $(DESTDIR)$(PREFIX)/bin
DESTMANDIR = $(DESTDIR)$(PREFIX)/share/man/man1
PERMREG    = 644
PERMEXE    = 755

# Phony targets

.PHONY: all clean help install $(ALLNAME:%=install-%) uninstall $(ALLNAME:%=uninstall-%) test-$(ASMNAME)

all: $(ALLBIN:%=$(BINDIR)/%)

clean:
	-rm -rf $(OBJDIR) $(ALLBIN:%=$(BINDIR)/%)

help:
	@echo
	@echo "  Target         Description"
	@echo "  -------------  ----------------------"
	@echo "                 Build all"
	@echo "  $(ASMBIN)        Build $(ASMBIN) only"
	@echo "  $(EMUBIN)        Build $(EMUBIN) only"
	@echo "  install        Install all"
	@echo "  install-$(ASMNAME)    Install $(ASMBIN) only"
	@echo "  install-$(EMUNAME)    Install $(EMUBIN) only"
	@echo "  uninstall      Uninstall all"
	@echo "  uninstall-$(ASMNAME)  Uninstall $(ASMBIN) only"
	@echo "  uninstall-$(EMUNAME)  Uninstall $(EMUBIN) only"
	@echo "  test-$(ASMNAME)       Test $(ASMBIN)"
	@echo "  clean          Clean built files"
	@echo "  $@           Display help"
	@echo

install: $(ALLNAME:%=install-%)

install-$(ASMNAME): $(ASMINSTALL)

install-$(EMUNAME): $(EMUINSTALL)

uninstall: $(ALLNAME:%=uninstall-%)

uninstall-$(ASMNAME):
	-rm -f $(ASMINSTALL)

uninstall-$(EMUNAME):
	-rm -f $(EMUINSTALL)

test-$(ASMNAME): $(ASMBIN)
	-$(TESTDIR)/$(ASMNAME)/test.sh $(BINDIR)/$(ASMBIN)

# File targets

$(BINDIR)/$(ASMBIN): $(ASMOBJS:%=$(OBJDIR)/%)
	$(CC) $(LDFLAGS) $^ -o $@

$(BINDIR)/$(EMUBIN): $(EMUOBJS:%=$(OBJDIR)/%)
	$(CC) $(LDFLAGS) $^ -o $@ -lcurses

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(DESTBINDIR)/%: $(BINDIR)/%
	mkdir -p $(dir $@)
	cp -f $< $@ && chmod $(PERMEXE) $@

$(DESTMANDIR)/%: $(MANDIR)/%
	mkdir -p $(dir $@)
	cp -f $< $@ && chmod $(PERMREG) $@
