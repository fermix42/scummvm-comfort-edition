MODULE := engines/gooka

MODULE_OBJS := \
	actor.o \
	devtools.o \
	effects.o \
	font.o \
	game.o \
	gooka.o \
	image.o \
	inventory.o \
	metaengine.o \
	music.o \
	primitives.o \
	resource.o \
	rooms/ported.o \
	rooms/room_data.o \
	rooms/rooms.o \
	saveload.o \
	scene.o \
	sequencer.o \
	sound.o \
	state.o \
	text.o \
	ui.o \
	walk.o

# Ported rooms: one file per original room function (rooms/room_<id>.cpp)
MODULE_OBJS += \
	rooms/room_0800.o \
	rooms/room_0809.o \
	rooms/room_0817.o \
	rooms/room_0854.o \
	rooms/room_0859.o \
	rooms/room_0873.o \
	rooms/room_0923.o \
	rooms/room_0927.o \
	rooms/room_1200.o \
	rooms/room_1209.o \
	rooms/room_1211.o \
	rooms/room_1213.o \
	rooms/room_1215.o \
	rooms/room_1217.o \
	rooms/room_1221.o \
	rooms/room_1223.o \
	rooms/room_1239.o \
	rooms/room_1334.o \
	rooms/room_1383.o \
	rooms/room_1600.o \
	rooms/room_1602.o \
	rooms/room_1604.o \
	rooms/room_1606.o \
	rooms/room_1608.o \
	rooms/room_1617.o \
	rooms/room_1619.o \
	rooms/room_1679.o \
	rooms/room_1705.o \
	rooms/room_1708.o \
	rooms/room_1719.o \
	rooms/room_2000.o \
	rooms/room_2037.o \
	rooms/room_2049.o \
	rooms/room_2078.o \
	rooms/room_2083.o \
	rooms/room_2099.o \
	rooms/room_2100.o \
	rooms/room_2101.o \
	rooms/room_2102.o \
	rooms/room_2103.o \
	rooms/room_2104.o \
	rooms/room_2105.o \
	rooms/room_2106.o \
	rooms/room_2116.o \
	rooms/room_2300.o \
	rooms/room_2304.o \
	rooms/room_2312.o \
	rooms/room_2322.o \
	rooms/room_2400.o \
	rooms/room_2410.o \
	rooms/room_2415.o \
	rooms/room_2419.o \
	rooms/room_2424.o \
	rooms/room_2425.o \
	rooms/room_2431.o \
	rooms/room_2444.o \
	rooms/room_2448.o \
	rooms/room_2600.o \
	rooms/room_2601.o \
	rooms/room_2610.o \
	rooms/room_2613.o \
	rooms/room_2615.o \
	rooms/room_2619.o \
	rooms/room_2626.o \
	rooms/room_2627.o \
	rooms/room_2628.o \
	rooms/room_2629.o \
	rooms/room_2630.o \
	rooms/room_2631.o \
	rooms/room_2632.o \
	rooms/room_2633.o \
	rooms/room_2634.o \
	rooms/room_2635.o \
	rooms/room_2640.o \
	rooms/room_2641.o \
	rooms/room_2642.o \
	rooms/room_2643.o \
	rooms/room_2644.o \
	rooms/room_2645.o \
	rooms/room_2646.o \
	rooms/room_2647.o \
	rooms/room_2648.o \
	rooms/room_2649.o \
	rooms/room_2650.o \
	rooms/room_2651.o \
	rooms/room_2652.o \
	rooms/room_2653.o \
	rooms/room_2654.o \
	rooms/room_2655.o \
	rooms/room_2656.o \
	rooms/room_2657.o \
	rooms/room_2658.o \
	rooms/room_3200.o \
	rooms/room_3226.o \
	rooms/room_3229.o \
	rooms/room_3243.o \
	rooms/room_3246.o \
	rooms/room_4219e0.o \
	rooms/room_422050.o \
	rooms/room_template_419.o \
	rooms/room_template_421a20.o

# This module can be built as a plugin
ifeq ($(ENABLE_GOOKA), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
