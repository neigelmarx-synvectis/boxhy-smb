# Tests unitaires hôte (gcc) du firmware BOXHY SMB.
#   make -C tests          : vérifie la base CAN générée puis compile et exécute tous les tests
#   make -C tests clean
# Les modules sont compilés avec les doubles de la couche HAL (tests/mocks) à la
# place de l'implémentation cible (hal/) : tests/mocks est cherché en premier.

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -g -O0
CFLAGS  += -I mocks -I ../src -I ..
LDLIBS   = -lm

SRC   = ../src
BUILD = build
MOCKS = mocks/hal/mocks.c
PYTHON ?= python3
# Toute modification d'un en-tête (dont src/can_db.h généré) recompile les tests.
HEADERS = $(wildcard $(SRC)/*.h) $(wildcard mocks/hal/*.h) test_framework.h

TESTS = can_signal generic timebase ctn_sensor measures cooling fcs battery ems inverter hmi maintenance \
        can_rx can_tx smb_state_machine

all: candb-check $(TESTS:%=run-%)

# src/can_db.h et les .DBF doivent correspondre à can database/BOXHY.dbc
candb-check:
	@$(PYTHON) ../tools/candb_gen.py --check

$(TESTS:%=run-%): run-%: $(BUILD)/test_%
	@./$<

$(BUILD):
	@mkdir -p $(BUILD)

define TEST_RULE
$(BUILD)/test_$(1): test_$(1).c $(2) $(MOCKS) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -o $$@ test_$(1).c $(2) $(MOCKS) $(LDLIBS)
endef

$(eval $(call TEST_RULE,can_signal,$(SRC)/can_signal.c))
$(eval $(call TEST_RULE,generic,$(SRC)/generic.c))
$(eval $(call TEST_RULE,timebase,$(SRC)/timebase.c))
$(eval $(call TEST_RULE,ctn_sensor,$(SRC)/ctn_sensor.c))
$(eval $(call TEST_RULE,measures,$(SRC)/measures.c $(SRC)/ctn_sensor.c))
$(eval $(call TEST_RULE,cooling,$(SRC)/cooling.c $(SRC)/generic.c))
$(eval $(call TEST_RULE,fcs,$(SRC)/fcs.c $(SRC)/can_signal.c $(SRC)/generic.c))
$(eval $(call TEST_RULE,battery,$(SRC)/battery.c $(SRC)/can_signal.c $(SRC)/generic.c))
$(eval $(call TEST_RULE,ems,$(SRC)/ems.c $(SRC)/can_signal.c $(SRC)/fcs.c $(SRC)/generic.c))
$(eval $(call TEST_RULE,inverter,$(SRC)/inverter.c $(SRC)/generic.c))
$(eval $(call TEST_RULE,hmi,$(SRC)/hmi.c $(SRC)/can_signal.c $(SRC)/battery.c $(SRC)/inverter.c $(SRC)/fcs.c $(SRC)/generic.c))
$(eval $(call TEST_RULE,maintenance,$(SRC)/maintenance.c $(SRC)/can_signal.c))
$(eval $(call TEST_RULE,can_rx,$(SRC)/can_rx.c $(SRC)/can_signal.c $(SRC)/generic.c $(SRC)/timebase.c $(SRC)/ctn_sensor.c $(SRC)/measures.c $(SRC)/battery.c $(SRC)/fcs.c $(SRC)/ems.c $(SRC)/inverter.c $(SRC)/hmi.c $(SRC)/maintenance.c $(SRC)/cooling.c $(SRC)/smb_state_machine.c))
$(eval $(call TEST_RULE,can_tx,$(SRC)/can_tx.c $(SRC)/can_signal.c $(SRC)/generic.c $(SRC)/timebase.c $(SRC)/ctn_sensor.c $(SRC)/measures.c $(SRC)/battery.c $(SRC)/fcs.c $(SRC)/ems.c $(SRC)/inverter.c $(SRC)/hmi.c $(SRC)/maintenance.c $(SRC)/cooling.c $(SRC)/smb_state_machine.c))
$(eval $(call TEST_RULE,smb_state_machine,$(SRC)/can_signal.c $(SRC)/generic.c $(SRC)/timebase.c $(SRC)/ctn_sensor.c $(SRC)/measures.c $(SRC)/battery.c $(SRC)/fcs.c $(SRC)/ems.c $(SRC)/inverter.c $(SRC)/hmi.c $(SRC)/maintenance.c $(SRC)/cooling.c $(SRC)/smb_state_machine.c))

clean:
	rm -rf $(BUILD)

.PHONY: all clean candb-check $(TESTS:%=run-%)
