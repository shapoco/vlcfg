.PHONY: all build clean distclean

#BOARD := pico
#BOARD := pico2
BOARD := pico_w
#BOARD := pico2_w

APP_NAME = vlcfg_test
REPO_DIR = $(shell git rev-parse --show-toplevel)
BUILD_DIR = build/$(BOARD)
BIN_DIR = bin/$(BOARD)

CMD_INCR_REV = python3 $(REPO_DIR)/bin/increment_revision.py

APP_UF2 = $(BIN_DIR)/$(APP_NAME).uf2
APP_ELF = $(BIN_DIR)/$(APP_NAME).elf

APP_INC_DIR = include
APP_SRC_DIR = src
APP_HTTPD_CONTENT_DIR = content
APP_HPP_LIST = \
	$(wildcard $(APP_INC_DIR)/*.hpp) \
	$(wildcard $(APP_INC_DIR)/*.h) \
	$(wildcard $(APP_INC_DIR)/ssd1306/*.hpp)
APP_CPP_LIST = \
	$(wildcard $(APP_SRC_DIR)/*.cpp) \
	$(wildcard $(APP_SRC_DIR)/*.c)

VLCFG_DIR = ../../lib
VLCFG_INC_DIR = $(VLCFG_DIR)/include
VLCFG_SRC_DIR = $(VLCFG_DIR)/src

DEPENDENCY_LIST = \
	$(APP_HPP_LIST) \
	$(APP_CPP_LIST) \
	$(wildcard $(VLCFG_INC_DIR)/vlcfg/*.hpp) \
	$(wildcard $(VLCFG_SRC_DIR)/*.cpp) \
	$(wildcard $(APP_HTTPD_CONTENT_DIR)/*.*) \
	Makefile

all: build
build: $(APP_UF2)

$(APP_UF2): $(DEPENDENCY_LIST) CMakeLists.txt
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) \
		&& cmake -DPICO_BOARD=$(BOARD) -DCMAKE_BUILD_TYPE=Debug ../.. \
		&& make -j
	@mkdir -p $(BIN_DIR)
	@cp $(BUILD_DIR)/$(APP_NAME).uf2 $(BIN_DIR)/.
	@cp $(BUILD_DIR)/$(APP_NAME).elf $(BIN_DIR)/.
	@echo "------------------------------"
	@grep "todo" -r $(APP_SRC_DIR) $(APP_INC_DIR) || true
	@echo "------------------------------"
	@echo -n "UF2 File: "
	@du -h $(APP_UF2)
	@echo -n "ELF File: "
	@du -h $(APP_ELF)

$(APP_ELF): $(APP_UF2)

PICO_DRIVE := e

flash: $(APP_UF2)
	sudo mkdir -p /mnt/$(PICO_DRIVE)
	sudo mount -t drvfs $(PICO_DRIVE): /mnt/$(PICO_DRIVE)
	cp $(APP_UF2) /mnt/$(PICO_DRIVE)/.

objclean:
	rm -f \
		$(BUILD_DIR)/*.uf2 \
		$(BUILD_DIR)/*.elf

clean: objclean
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf $(BIN_DIR)
