/**
 * \file goto.c
 *
 * \date 02.07.2009
 * \author kse
 */
#include "shell_command.h"

#define COMMAND_NAME     "exec"
#define COMMAND_DESC_MSG "execute image file"
#define HELP_MSG         "Usage: goto [-h] [-a addr]"
static const char *man_page =
	#include "goto_help.inc"
	;

DECLARE_SHELL_COMMAND_DESCRIPTOR(COMMAND_NAME, exec, COMMAND_DESC_MSG, HELP_MSG, man_page);

typedef void (*IMAGE_ENTRY)();

void go_to(unsigned int addr) {
	printf("Try goto 0x%08X\n", addr);
	/*__asm__ __volatile__("jmpl %0, %%g0\nnop\n"::"rI"(addr));
    */
	timers_off();
	irq_ctrl_disable_all();
	((IMAGE_ENTRY)addr)();
}

static int exec(int argsc, char **argsv){
	int nextOption;
	unsigned int addr = 0;
	getopt_init();
	do {
	        nextOption = getopt(argsc, argsv, "a:h");
	        switch(nextOption) {
	        case 'h':
	                show_help();
	                return 0;
	        case 'a':
			if ((optarg == NULL) || (*optarg == '\0')) {
				LOG_ERROR("goto: missed address value\n");
				return -1;
			}
			if (sscanf(optarg, "0x%x", &addr) > 0) {
				go_to(addr);
				return 0;
			}
			LOG_ERROR("goto: invalid value \"%s\".\n\the number expected.\n", optarg);
			return -1;
		case -1:
		        break;
		default:
		        return 0;
		}
	} while(-1 != nextOption);

	return 0;
}
