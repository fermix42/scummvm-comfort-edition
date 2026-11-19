MODULE := engines/nox
MODULE_OBJS := metaengine.o native_engine.o mockingboard.o enhancements.o speaker_audio.o
MODULE_DIRS += engines/nox
ifeq ($(ENABLE_NOX), DYNAMIC_PLUGIN)
PLUGIN := 1
endif
include $(srcdir)/rules.mk
DETECT_OBJS += $(MODULE)/detection.o
