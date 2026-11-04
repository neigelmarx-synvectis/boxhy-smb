# Tests unitaires hôte (gcc) du firmware BOXHY SMB.
#   make -C tests          : vérifie la base CAN générée puis compile et exécute tous les tests
#   make -C tests clean
# Les modules sont compilés avec les doubles de la couche HAL (tests/mocks) à la
# place de l'implémentation cible (hal/) : tests/mocks est cherché en premier.

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -g -O0
CFLAGS  += -I mocks -I ..   # includes écrits depuis la racine : "src/<dossier>/x.h", "hal/x.h"
LDLIBS   = -lm

SRC   = ../src
BUILD = build
MOCKS = mocks/hal/mocks.c
PYTHON ?= python3
# Toute modification d'un en-tête (dont src/can/can_db.h généré) recompile les tests.
HEADERS = $(wildcard $(SRC)/*.h $(SRC)/*/*.h) $(wildcard mocks/hal/*.h) test_framework.h

TESTS = can_signal generic timebase ctn_sensor measures cooling fcs battery ems inverter hmi maintenance \
        can_rx can_tx smb_state_machine

all: candb-check $(TESTS:%=run-%)

# src/can/can_db.h et les .DBF doivent correspondre à can database/BOXHY.dbc
candb-check:
	@$(PYTHON) ../tools/candb_gen.py --check
	@$(PYTHON) ../tools/powermodule_dbc.py --check

$(TESTS:%=run-%): run-%: $(BUILD)/test_%
	@./$<

$(BUILD):
	@mkdir -p $(BUILD)

define TEST_RULE
$(BUILD)/test_$(1): test_$(1).c $(2) $(MOCKS) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -o $$@ test_$(1).c $(2) $(MOCKS) $(LDLIBS)
endef

$(eval $(call TEST_RULE,can_signal,$(SRC)/can/can_signal.c))
$(eval $(call TEST_RULE,generic,$(SRC)/utils/generic.c))
$(eval $(call TEST_RULE,timebase,$(SRC)/system/timebase.c))
$(eval $(call TEST_RULE,ctn_sensor,$(SRC)/measures/ctn_sensor.c))
$(eval $(call TEST_RULE,measures,$(SRC)/measures/measures.c $(SRC)/measures/ctn_sensor.c))
$(eval $(call TEST_RULE,cooling,$(SRC)/control/cooling.c $(SRC)/utils/generic.c))
$(eval $(call TEST_RULE,fcs,$(SRC)/devices/fcs.c $(SRC)/can/can_signal.c $(SRC)/utils/generic.c))
$(eval $(call TEST_RULE,battery,$(SRC)/devices/battery.c $(SRC)/can/can_signal.c $(SRC)/utils/generic.c))
$(eval $(call TEST_RULE,ems,$(SRC)/control/ems.c $(SRC)/can/can_signal.c $(SRC)/devices/fcs.c $(SRC)/utils/generic.c))
$(eval $(call TEST_RULE,inverter,$(SRC)/devices/inverter.c $(SRC)/utils/generic.c))
$(eval $(call TEST_RULE,hmi,$(SRC)/hmi/hmi.c $(SRC)/can/can_signal.c $(SRC)/devices/battery.c $(SRC)/devices/inverter.c $(SRC)/devices/fcs.c $(SRC)/utils/generic.c))
$(eval $(call TEST_RULE,maintenance,$(SRC)/hmi/maintenance.c $(SRC)/can/can_signal.c))
$(eval $(call TEST_RULE,can_rx,$(SRC)/can/can_rx.c $(SRC)/can/can_signal.c $(SRC)/utils/generic.c $(SRC)/system/timebase.c $(SRC)/measures/ctn_sensor.c $(SRC)/measures/measures.c $(SRC)/devices/battery.c $(SRC)/devices/fcs.c $(SRC)/control/ems.c $(SRC)/devices/inverter.c $(SRC)/hmi/hmi.c $(SRC)/hmi/maintenance.c $(SRC)/control/cooling.c $(SRC)/system/smb_state_machine.c))
$(eval $(call TEST_RULE,can_tx,$(SRC)/can/can_tx.c $(SRC)/can/can_signal.c $(SRC)/utils/generic.c $(SRC)/system/timebase.c $(SRC)/measures/ctn_sensor.c $(SRC)/measures/measures.c $(SRC)/devices/battery.c $(SRC)/devices/fcs.c $(SRC)/control/ems.c $(SRC)/devices/inverter.c $(SRC)/hmi/hmi.c $(SRC)/hmi/maintenance.c $(SRC)/control/cooling.c $(SRC)/system/smb_state_machine.c))
$(eval $(call TEST_RULE,smb_state_machine,$(SRC)/can/can_signal.c $(SRC)/utils/generic.c $(SRC)/system/timebase.c $(SRC)/measures/ctn_sensor.c $(SRC)/measures/measures.c $(SRC)/devices/battery.c $(SRC)/devices/fcs.c $(SRC)/control/ems.c $(SRC)/devices/inverter.c $(SRC)/hmi/hmi.c $(SRC)/hmi/maintenance.c $(SRC)/control/cooling.c $(SRC)/system/smb_state_machine.c))

clean:
	rm -rf $(BUILD)

.PHONY: all clean candb-check $(TESTS:%=run-%)
