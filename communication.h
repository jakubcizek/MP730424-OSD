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

#define CMD_IDENTIFICATION "*IDN?\n"
#define CMD_IDENTIFICATION_SIZE 6

#define CMD_FN "FUNC?\n"
#define CMD_FN_SIZE 6

#define CMD_MEASUREMENT "MEAS?\n"
#define CMD_MEASUREMENT_SIZE 6

#define FN_DIODE 0
#define FN_CONTIINUITY 1
#define FN_VOLTAGE_AC 2
#define FN_VOLTAGE_DC 3
#define FN_CURRENT 4
#define FN_RESISTANCE 5
#define FN_CAPACITANCE 6
#define FN_FREQUENCY 7
#define FN_TEMPERATURE 8

#define FN_NAME_DIODE "Dioda"
#define FN_NAME_CONTIINUITY "Kontinuita"
#define FN_NAME_VOLTAGE_AC "Střídavé napětí"
#define FN_NAME_VOLTAGE_DC "Stejnosměrné napětí"
#define FN_NAME_CURRENT "Proud"
#define FN_NAME_RESISTANCE "Odpor"
#define FN_NAME_CAPACITANCE "Kapacita"
#define FN_NAME_FREQUENCY "Frekvence"
#define FN_NAME_TEMPERATURE "Teplota"

#define MAX_LABEL_SIZE 200

struct serial_params_s
{
	char *device;
	int fd, n;
	int cnt, size, s_cnt;
	struct termios oldtp, newtp;
};

typedef struct Config
{
	uint16_t error_flag;
	char logfilename[30];
	struct serial_params_s serial;
	int serial_timeout;
	int interval;
	char *decimal_point;
} Config;

extern void multimeter_communication_init(int argc, char **argv);
extern void multimeter_open_port();
extern void multimeter_close_port();
extern void multimeter_ping(gchar *response);
static ssize_t serial_write_data(char *d, ssize_t s);
static ssize_t serial_read_data(char *b, ssize_t s);
extern void multimeter_read(uint8_t *function, double *value, gchar *units);
extern gchar fn_names[9][50];
extern Config config;

#endif