DEFERRED_EXEC_ENABLE = yes
AUTO_SHIFT_ENABLE = yes
REPEAT_KEY_ENABLE = yes
ALLOW_WARNINGS = yes

# Apple Clang host compiler flags to avoid errors on upstream QMK code
CFLAGS += -Wno-include-next-absolute-path -Wno-strict-prototypes
CXXFLAGS += -Wno-c++11-narrowing

SRC += hrm.c
