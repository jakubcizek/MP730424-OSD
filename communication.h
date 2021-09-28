#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <gtk/gtk.h>
#include <termios.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <locale.h>

#define uu "\u00B5"
#define dd "\u00B0"
#define oo "\u03A9"

#define FN_DIODE       0
#define FN_CONTIINUITY 1
#define FN_VOLTAGE_AC  2
#define FN_VOLTAGE_DC  3
#define FN_CURRENT     4
#define FN_RESISTANCE  5
#define FN_CAPACITANCE 6
#define FN_FREQUENCY   7
#define FN_TEMPERATURE 8

#define FN_NAME_DIODE       "Dioda"
#define FN_NAME_CONTIINUITY "Kontinuita"
#define FN_NAME_VOLTAGE_AC  "Střídavé napětí"
#define FN_NAME_VOLTAGE_DC  "Stejnosměrné napětí"
#define FN_NAME_CURRENT     "Proud"
#define FN_NAME_RESISTANCE  "Odpor"
#define FN_NAME_CAPACITANCE "Kapacita"
#define FN_NAME_FREQUENCY   "Frekvence"
#define FN_NAME_TEMPERATURE "Teplota"

extern gchar fn_names[9][50];

struct serial_params_s {
	char *device;
	int fd, n;
	int cnt, size, s_cnt;
	struct termios oldtp, newtp;
};

typedef struct glb {
	uint16_t error_flag;
	char get_func[7];
	char meas_value[7];
	char logfilename[30];
	struct serial_params_s serial;
	int serial_timeout;
	int interval;
	char *decimal_point;
} glb;

glb g;                

extern void multimeter_communication_init(int argc, char **argv);
extern void multimeter_open_port();
static ssize_t serial_write_data(char *d, ssize_t s );
static ssize_t serial_read_data(char *b, ssize_t s );
extern void multimeter_read(uint8_t *function, double *value, gchar *units);

#endif