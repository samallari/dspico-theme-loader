NAME        := _pico-theme-loader
GAME_TITLE  := Pico Theme Loader
GAME_SUBTITLE := for Pico Launcher
GAME_AUTHOR := smoewoe

NITROFSDIR  :=

SOURCEDIRS  := source
INCLUDEDIRS :=
BINDIRS     :=

LIBS        := -lnds9
LIBDIRS     := $(BLOCKSDS)/libs/libnds

ARM7ELF     := $(BLOCKSDS)/sys/arm7/main_core/arm7_minimal.elf

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile