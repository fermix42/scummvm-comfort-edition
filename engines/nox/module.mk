MODULE := engines/nox
MODULE_OBJS := metaengine.o nox.o validation.o session_validation.o runtime.o runtime_state.o render.o movement.o speaker_audio.o mockingboard.o enhancements.o runtime_observers.o
MODULE_DIRS += engines/nox
ifeq ($(ENABLE_NOX), DYNAMIC_PLUGIN)
PLUGIN := 1
endif
include $(srcdir)/rules.mk
DETECT_OBJS += $(MODULE)/detection.o
