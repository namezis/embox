/**
 * \file mem.c
 *
 * \date 13.02.2009
 * \author Alexey Fomin
 */
#include "shell_command.h"

#define COMMAND_NAME     "mem"
#define COMMAND_DESC_MSG "read from memory"
#define HELP_MSG         "Usage: mem [-h] [-a addr] [-n num]"
static const char *man_page =
	#include "mem_help.inc"
	;

DECLARE_SHELL_COMMAND_DESCRIPTOR(COMMAND_NAME, exec, COMMAND_DESC_MSG, HELP_MSG, man_page);

char mem_keys[] = {
	'a', 'n', 'h'
};

void mem_print(WORD *addr, long int amount) {
	long i = 0;
	addr = (WORD *) ((WORD) addr & 0xFFFFFFFC);
	while (i < amount) {
		if (0 == (i % 4)) {
			printf("0x%08x:\t", (int) (addr + i));
		}
		printf("0x%08x\t", *(addr + i));
		if (3 == (i % 4)) {
			printf("\n");
		}
		i++;
	}
}

typedef void TEST_MEM_FUNC(WORD *addr, long int amount);

static int exec(int argsc, char **argsv) {
        WORD *address = (WORD *) 0x70000000L;
        long int amount = 100L;
        int nextOption;
        getopt_init();
        do {
                nextOption = getopt(argsc, argsv, "a:h");
                switch(nextOption) {
                case 'h':
                        show_help();
                        return 0;
                case 'a':
            		if (optarg == NULL) {
                                LOG_ERROR("mem: -a: hex value for address expected.\n");
                                show_help();
                                return -1;
                        }
                        if (!sscanf(optarg, "0x%x", &address)) {
                                LOG_ERROR("mem: -a: hex value for address expected.\n");
                                show_help();
                                return -1;
                        }
                        break;
                case 'n':
            		if (optarg == NULL) {
                                LOG_ERROR("mem: -n: hex or integer value for number of bytes expected.\n");
                                show_help();
                                return -1;
                        }
                        if (!sscanf(optarg, "0x%x", amount)) {
                                if (!sscanf(optarg, "%d", &amount)) {
                                        LOG_ERROR("mem: -n: hex or integer value for number of bytes expected.\n");
                                        show_help();
                                        return -1;
                                }
                        }
                        break;
                case -1:
                        break;
                default:
                	return 0;
                }
        } while(-1 != nextOption);

	mem_print(address, amount);
	return 0;
}
