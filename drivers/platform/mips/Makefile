obj-y += acpi_init.o cpu_hwmon.o
obj-$(CONFIG_I2C_PIIX4) += emc1412.o tmp75.o
obj-$(CONFIG_CPU_LOONGSON3) += wpce_fan.o sbx00_fan.o
obj-$(CONFIG_LEMOTE3A_LAPTOP) += lemote3a-laptop.o
obj-m += ec_rom.o at24c04.o
obj-$(CONFIG_LOONGSON_LAPTOP) += loongson-czc-laptop.o

ifdef CONFIG_MTD
obj-m += pmon_flash.o
endif
