#include <stdio.h>

#include "varnishadm.h"


int main(int argc, char **argv) {
	int sock;
	struct vadmin_ret vret;
	sock = vadmin_init(argc,argv);
	vadmin_run(sock,"ping",&vret);
	printf("Return: %d msg: %s\n", vret.status, vret.answer);
	vadmin_run(sock,"help",&vret);
	printf("Return: %d msg: %s\n", vret.status, vret.answer);
	return 0;
}
