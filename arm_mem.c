#include <stdio.h>

#include "arm.h"
#include "arm_mem.h"

#include "io.h"

#define EEPROM_WRITE  2
#define EEPROM_READ   3

uint32_t flash_bank = 0;

typedef enum {
	IDLE,
	ERASE,
	WRITE,
	BANK_SWITCH
} flash_mode_e;

flash_mode_e flash_mode = IDLE;

bool flash_id_mode = false;

uint8_t eeprom_buff[0x100];

static const uint8_t bus_size_lut[16]  = { 4, 4, 2, 4, 4, 2, 2, 4, 2, 2, 2, 2, 2, 2, 1, 1 };

static void arm_access(uint32_t address, access_type_e at) {
	uint8_t cycles = 1;

	if (address & 0x08000000) {
		if (at == NON_SEQ)
			cycles += ws_n[(address >> 25) & 3];
		else
			cycles += ws_s[(address >> 25) & 3];
	} else if ((address >> 24) == 2) {
		cycles += 2;
	}

	arm_cycles += cycles;
}

void arm_access_bus(uint32_t address, uint8_t size, access_type_e at) {
	uint8_t lut_idx = (address >> 24) & 0xf;
	uint8_t bus_sz = bus_size_lut[lut_idx];

	if (bus_sz < size) {
		arm_access(address + 0, at);
		arm_access(address + 2, SEQUENTIAL);
	} else {
		arm_access(address, at);
	}
}

//Memory read
static void arm_read_bios(uint32_t address) {
	if ((address | arm_r.r[15]) < 0x4000) arm_bus = bios[address & 0x3fff];
}

static void arm_read_wram(uint32_t address) {
	arm_bus = wram[address & 0x3ffff];
}

static void arm_read_iwram(uint32_t address) {
	arm_bus = iwram[address & 0x7fff];
}

static void arm_read_pram(uint32_t address) {
	arm_bus = pram[address & 0x3ff];
}

static void arm_read_vram(uint32_t address) {
	arm_bus = vram[address & (address & 0x10000 ? 0x17fff : 0x1ffff)];
}

static void arm_read_oam(uint32_t address) {
	arm_bus = oam[address & 0x3ff];
}

static void arm_read_rom(uint32_t address) {
	arm_bus = rom[address & 0x1ffffff];
}

static void arm_read_flash(uint32_t address) {
	if (flash_id_mode) {
		//This is the Flash ROM ID, we return Sanyo ID code
		switch (address) {
			case 0x0e000000: arm_bus = 0x62; break;
			case 0x0e000001: arm_bus = 0x13; break;
		}
	} else {
		arm_bus = flash[flash_bank | (address & 0xffff)];
	}
}

static uint8_t arm_read_(uint32_t address) {
	switch (address >> 24) {
		case 0x0: arm_read_bios(address);     break;
		case 0x2: arm_read_wram(address);     break;
		case 0x3: arm_read_iwram(address);    break;
		case 0x4: arm_bus = io_read(address); break;
		case 0x5: arm_read_pram(address);     break;
		case 0x6: arm_read_vram(address);     break;
		case 0x7: arm_read_oam(address);      break;

		case 0x8:
		case 0x9:
			arm_read_rom(address); break;

		case 0xa:
		case 0xb:
			arm_read_rom(address); break;

		case 0xc:
		case 0xd:
			arm_read_rom(address); break;

		case 0xe:
		case 0xf:
			arm_read_flash(address); break;
	}

	return arm_bus;
}

static uint8_t arm_readb(uint32_t address) {
	return arm_read_(address);
}

static uint32_t arm_readh(uint32_t address) {
	uint32_t a = address & ~1;
	uint8_t  s = address &  1;

	uint32_t value =
		arm_read_(a | 0) << 0 |
		arm_read_(a | 1) << 8;

	return ROR(value, s << 3);
}

static uint32_t arm_read(uint32_t address) {
	uint32_t a = address & ~3;
	uint8_t  s = address &  3;

	uint32_t value =
		arm_read_(a | 0) <<  0 |
		arm_read_(a | 1) <<  8 |
		arm_read_(a | 2) << 16 |
		arm_read_(a | 3) << 24;

	return ROR(value, s << 3);
}

uint8_t arm_readb_n(uint32_t address) {
	arm_access_bus(address, ARM_BYTE_SZ, NON_SEQ);

	return arm_readb(address);
}

uint32_t arm_readh_n(uint32_t address) {
	arm_access_bus(address, ARM_HWORD_SZ, NON_SEQ);

	return arm_readh(address);
}

uint32_t arm_read_n(uint32_t address) {
	arm_access_bus(address, ARM_WORD_SZ, NON_SEQ);

	return arm_read(address);
}

uint8_t arm_readb_s(uint32_t address) {
	arm_access_bus(address, ARM_BYTE_SZ, SEQUENTIAL);

	return arm_readb(address);
}

uint32_t arm_readh_s(uint32_t address) {
	arm_access_bus(address, ARM_HWORD_SZ, SEQUENTIAL);

	return arm_readh(address);
}

uint32_t arm_read_s(uint32_t address) {
	arm_access_bus(address, ARM_WORD_SZ, SEQUENTIAL);

	return arm_read(address);
}

//Memory write
static void arm_write_wram(uint32_t address, uint8_t value) {
	wram[address & 0x3ffff] = value;
}

static void arm_write_iwram(uint32_t address, uint8_t value) {
	iwram[address & 0x7fff] = value;
}

static void arm_write_pram(uint32_t address, uint8_t value) {
	pram[address & 0x3ff] = value;

	address &= 0x3fe;

	uint16_t pixel = pram[address] | (pram[address + 1] << 8);

	uint8_t r = ((pixel >>  0) & 0x1f) << 3;
	uint8_t g = ((pixel >>  5) & 0x1f) << 3;
	uint8_t b = ((pixel >> 10) & 0x1f) << 3;

	uint32_t rgba = 0xff;

	rgba |= (r | (r >> 5)) <<  8;
	rgba |= (g | (g >> 5)) << 16;
	rgba |= (b | (b >> 5)) << 24;

	palette[address >> 1] = rgba;
}

static void arm_write_vram(uint32_t address, uint8_t value) {
	vram[address & (address & 0x10000 ? 0x17fff : 0x1ffff)] = value;
}

static void arm_write_oam(uint32_t address, uint8_t value) {
	oam[address & 0x3ff] = value;
}

static void arm_write_flash(uint32_t address, uint8_t value) {
	if (flash_mode == WRITE) {
		flash[address & 0xffff] = value;
	} else if (flash_mode == BANK_SWITCH && address == 0x0e000000) {
		flash_bank = (value & 1) << 16;
	} else if (sram[0x5555] == 0xaa && sram[0x2aaa] == 0x55) {
		if (address == 0x0e005555) {
			//Command to do something on Flash ROM
			switch (value) {
				//Erase all
				case 0x10: 
					if (flash_mode == ERASE) {
						uint32_t idx;

						for (idx = 0; idx < 0x20000; idx++) {
							flash[idx] = 0xff;
						}
					}
				break;

				case 0x80: flash_mode    = ERASE;       break;
				case 0x90: flash_id_mode = true;        break;
				case 0xa0: flash_mode    = WRITE;       break;
				case 0xb0: flash_mode    = BANK_SWITCH; break;
				case 0xf0: flash_id_mode = false;       break;
			}
		} else if (flash_mode == ERASE && (address & 0xfff) == 0) {
			uint32_t bank_s = address & 0xf000;
			uint32_t bank_e = bank_s + 0x1000;
			uint32_t idx;

			for (idx = bank_s; idx < bank_e; idx++) {
				flash[idx] = 0xff;
			}
		}
	}

	sram[address & 0xffff] = value;

	flash_mode = IDLE;
}

static void arm_write_(uint32_t address, uint8_t value) {
	switch (address >> 24) {
		case 0x2: arm_write_wram(address, value);  break;
		case 0x3: arm_write_iwram(address, value); break;
		case 0x4: io_write(address, value);        break;
		case 0x5: arm_write_pram(address, value);  break;
		case 0x6: arm_write_vram(address, value);  break;
		case 0x7: arm_write_oam(address, value);   break;

		case 0xe:
		case 0xf:
			arm_write_flash(address, value); break;
	}
}

static void arm_writeb(uint32_t address, uint8_t value) {
	uint8_t ah = address >> 24;

	if (ah == 7) return; //OAM doesn't supposrt 8 bits writes

	if (ah > 4 && ah < 8) {
		arm_write_(address + 0, value);
		arm_write_(address + 1, value);
	} else {
		arm_write_(address, value);
	}
}

static void arm_writeh(uint32_t address, uint16_t value) {
	uint32_t a = address & ~1;

	arm_write_(a | 0, (uint8_t)(value >> 0));
	arm_write_(a | 1, (uint8_t)(value >> 8));
}

static void arm_write(uint32_t address, uint32_t value) {
	uint32_t a = address & ~3;

	arm_write_(a | 0, (uint8_t)(value >>  0));
	arm_write_(a | 1, (uint8_t)(value >>  8));
	arm_write_(a | 2, (uint8_t)(value >> 16));
	arm_write_(a | 3, (uint8_t)(value >> 24));
}

void arm_writeb_n(uint32_t address, uint8_t value) {
	arm_access_bus(address, ARM_BYTE_SZ, NON_SEQ);

	arm_writeb(address, value);
}

void arm_writeh_n(uint32_t address, uint16_t value) {
	arm_access_bus(address, ARM_HWORD_SZ, NON_SEQ);

	arm_writeh(address, value);
}

void arm_write_n(uint32_t address, uint32_t value) {
	arm_access_bus(address, ARM_WORD_SZ, NON_SEQ);

	arm_write(address, value);
}

void arm_writeb_s(uint32_t address, uint8_t value) {
	arm_access_bus(address, ARM_BYTE_SZ, SEQUENTIAL);

	arm_writeb(address, value);
}

void arm_writeh_s(uint32_t address, uint16_t value) {
	arm_access_bus(address, ARM_HWORD_SZ, SEQUENTIAL);

	arm_writeh(address, value);
}

void arm_write_s(uint32_t address, uint32_t value) {
	arm_access_bus(address, ARM_WORD_SZ, SEQUENTIAL);

	arm_write(address, value);
}