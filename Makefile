DRIVER_NAME := RKKDR
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
BUILD_DIR := $(PWD)/build
SRC_DIR := $(PWD)/src

RELEASE_DIR := $(BUILD_DIR)/release
KEYS_DIR := $(PWD)/keys

SUDO_CMD ?= sudo
-include .local.mk

all: check_unload compile sign compile_commands post_clean

check_unload:
	@if lsmod | grep -q "^$(DRIVER_NAME)\b"; then \
		echo "Module $(DRIVER_NAME) is loaded. Unloading first..."; \
		$(SUDO_CMD) rmmod $(DRIVER_NAME) || true; \
	fi

prep: check_unload
	@mkdir -p $(BUILD_DIR) $(RELEASE_DIR)
	@cp -r $(SRC_DIR)/* $(BUILD_DIR)/
	@echo "obj-m += $(DRIVER_NAME).o" > $(BUILD_DIR)/Makefile
	@echo "$(DRIVER_NAME)-objs := $$(find $(BUILD_DIR) -name '*.c' | sed "s|^$(BUILD_DIR)/||" | sed 's/\.c$$/.o/' | tr '\n' ' ')" >> $(BUILD_DIR)/Makefile

compile: prep
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) modules
	@cp $(BUILD_DIR)/$(DRIVER_NAME).ko $(RELEASE_DIR)/

sign: compile
	@mkdir -p $(KEYS_DIR)
	@if [ ! -f $(KEYS_DIR)/MOK.priv ] || [ ! -f $(KEYS_DIR)/MOK.der ]; then \
		echo "Generating MOK keys for module signing..."; \
		openssl req -new -x509 -newkey rsa:2048 -keyout $(KEYS_DIR)/MOK.priv -outform DER -out $(KEYS_DIR)/MOK.der -nodes -days 36500 -subj "/CN=$(DRIVER_NAME)/"; \
	fi
	$(KDIR)/scripts/sign-file sha256 $(KEYS_DIR)/MOK.priv $(KEYS_DIR)/MOK.der $(RELEASE_DIR)/$(DRIVER_NAME).ko

compile_commands: prep compile
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) compile_commands.json
	-@sed -i 's|$(BUILD_DIR)|$(SRC_DIR)|g' $(BUILD_DIR)/compile_commands.json

post_clean: sign compile_commands
	@echo "Cleanup Kbuild intermediate  files..."
	@find $(BUILD_DIR) -mindepth 1 -maxdepth 1 ! -name "release" ! -name "compile_commands.json" -exec rm -rf {} +

clean:
	@echo "Request removing build folder"
	@$(SUDO_CMD) rm -rf $(BUILD_DIR) 2>/dev/null || rm -rf $(BUILD_DIR)

load: all
	@echo "Unloading old driver (if it exists)..."
	@$(SUDO_CMD) rmmod $(DRIVER_NAME) 2>/dev/null || true
	@echo "Loading new driver..."
	@$(SUDO_CMD) insmod $(RELEASE_DIR)/$(DRIVER_NAME).ko
	@echo "Last 5 lines of kernel log:"
	@$(SUDO_CMD) dmesg | tail -n 5

unload:
	@$(SUDO_CMD) rmmod $(DRIVER_NAME)
