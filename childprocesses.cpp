#include <unistd.h>
#include <stdio.h>

int main() {
	int i = 0;
	for (i = 0; i < 3; i++) {
		if (fork () == 0)
			printf ("created\n");
	}
	return 0;
}
