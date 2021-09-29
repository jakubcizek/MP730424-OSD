
#include "communication.h"

gchar fn_names[9][50] = {
	FN_NAME_DIODE,     
	FN_NAME_CONTIINUITY, 
	FN_NAME_VOLTAGE_AC, 
	FN_NAME_VOLTAGE_DC,
	FN_NAME_CURRENT,    
	FN_NAME_RESISTANCE, 
	FN_NAME_CAPACITANCE, 
	FN_NAME_FREQUENCY,  
	FN_NAME_TEMPERATURE 
};

void multimeter_communication_init(int argc, char **argv) {
	g.error_flag = 0;
	g.interval = 100000;
	g.serial_timeout = 3;
	struct lconv *lc;
  	lc = localeconv();
  	g.decimal_point = lc->decimal_point;
	if(argc == 2) g.serial.device = argv[1];
  	else{
    	g_print("Uveď cestu k sériovému portu multimetru!\r\nTřeba: mp730424 /dev/ttyUSB0\r\n");
    	exit(1);
  	}
}

void multimeter_open_port() {
	int r; 

	g_print("Pokouším se otevřít %s ... ", g.serial.device);
	g.serial.fd = open( g.serial.device, O_RDWR | O_NOCTTY | O_NDELAY );
	fcntl(g.serial.fd,F_SETFL,0);
	tcgetattr(g.serial.fd,&(g.serial.oldtp)); 
	tcgetattr(g.serial.fd,&(g.serial.newtp));
	cfmakeraw(&(g.serial.newtp));

	g.serial.newtp.c_cflag = CS8 |  CLOCAL | CREAD ; 
	g.serial.newtp.c_cflag |= B115200; 
	g.serial.newtp.c_cc[VMIN] = 0;
	g.serial.newtp.c_cc[VTIME] = g.serial_timeout*10;
	g.serial.newtp.c_cflag &= ~(PARODD|PARENB);
	g.serial.newtp.c_cflag &= ~CSTOPB;
	g.serial.newtp.c_iflag &= ~(IXON | IXOFF | IXANY );

	if (tcsetattr(g.serial.fd, TCSANOW, &(g.serial.newtp))) {
		g_print("CHYBA!\r\n");
		g_print("Vybrali jste správné zařízení?\r\nKomunikuje rychlostí 115200 b/s a s konfigurací 8n1?\r\n");
		exit(1);
	}

	g_print("OK (FD = %d)\r\n", g.serial.fd);
}


void multimeter_close_port(){
	close(g.serial.fd);
}

static ssize_t serial_read_data(char *b, ssize_t s) {
	ssize_t sz;
	*b = '\0';
	int bp = 0;
	do {
		char temp_char;
		sz = read(g.serial.fd, &temp_char, 1);
		if (sz == -1) {
			b[0] = '\0';
			g_print("Nemohu přečíst data ze sériové linky\r\n");
			return sz;
		}
		if (sz > 0) {
			b[bp] = temp_char;
			if (b[bp] == '\n') break;
			bp++;
		} else b[bp] = '\0';
	} while (sz > 0 && bp < s);
	b[bp] = '\0';
	return sz;
}

static ssize_t serial_write_data(char *d, ssize_t s) { 
	ssize_t sz;
	sz = write(g.serial.fd, d, s); 
	if (sz < 0) {
		g.error_flag = 1;
		g_print("Chyba v odesílání sériových dat\r\n");
	}
	return sz;
}

void multimeter_ping(gchar *response){
	ssize_t sz = serial_write_data(CMD_IDENTIFICATION, CMD_IDENTIFICATION_SIZE);
	if(sz > -1){
		sz = serial_read_data(response, MAX_LABEL_SIZE);
	}
}

void multimeter_read(uint8_t *function, double *value, gchar *units){
	char buf_func[MAX_LABEL_SIZE];
	char buf_value[MAX_LABEL_SIZE];
	char buf_multiplier[10];

	char *p, *q;
	double v = 0.0;
	int ev = 0;
	uint8_t range;
	uint8_t dpp = 0;
	ssize_t bytes_read = 0;
	ssize_t sz;
    int flag_ol = 0;

    sz = serial_write_data(CMD_FN, CMD_FN_SIZE);
	if (sz > -1) {
		sz = serial_read_data(buf_func, sizeof(buf_func) );
		if (sz > -1) 
			if (strstr(buf_func,"E+") || strstr(buf_func,"E-")) sz = serial_read_data(buf_func, sizeof(buf_func) );
	}

	sz = serial_write_data(CMD_MEASUREMENT, CMD_MEASUREMENT_SIZE);
	if (sz > -1) {
		sz = serial_read_data(buf_value, sizeof(buf_value));
	}

    if (strncmp(buf_value, "1E+9", 4)==0) {
		flag_ol = 1;
	}
    else {
		char *e;
		flag_ol = 0;
        char *pos = strchr(buf_value, '.');
        if(pos) *pos = *g.decimal_point;
		*value = strtod(buf_value,NULL);
		e = strstr(buf_value,"E+");
		if (!e) e = strstr(buf_value, "E-");
		if (e) ev = strtol(e+2, NULL, 10);

		if (strncmp("\"DIOD",buf_func, 5)==0) {
			*function = FN_DIODE;
			g_snprintf(units,MAX_LABEL_SIZE ,"%s", oo);
		} 
		else if (strncmp("\"CONT",buf_func, 5)==0) {
			*function = FN_CONTIINUITY;
			g_snprintf(units,MAX_LABEL_SIZE ,"V");
		} 
		else if (strncmp("\"VOLT AC\"",buf_func, 6)==0) {
			*function = FN_VOLTAGE_AC;
			g_snprintf(units,MAX_LABEL_SIZE ,"V");
		} 
		else if (strncmp("\"VOLT\"",buf_func, 6)==0) {
			*function = FN_VOLTAGE_DC;
			g_snprintf(units,MAX_LABEL_SIZE ,"V");
		} 
		else if (strncmp("\"CURR",buf_func, 5)==0) {
			*function = FN_CURRENT;
			g_snprintf(units,MAX_LABEL_SIZE ,"A");
		} 
		else if (strncmp("\"RES",buf_func, 4)==0) {
			*function = FN_RESISTANCE;
			g_snprintf(units,MAX_LABEL_SIZE ,"%s", oo);
		} 
		else if (strncmp("\"CAP",buf_func, 4)==0) {
			*function = FN_CAPACITANCE;
			g_snprintf(units,MAX_LABEL_SIZE ,"F");
		} 
		else if (strncmp("\"FREQ",buf_func, 5)==0) {
			*function = FN_DIODE;
			g_snprintf(units,MAX_LABEL_SIZE ,"Hz");
		} 
		else if (strncmp("\"TEMP",buf_func, 5)==0) {
			*function = FN_TEMPERATURE;
			g_snprintf(units,MAX_LABEL_SIZE ,"%sC", dd);
		} 

		if (flag_ol == 0) {
		    switch (ev) {
			    case -12:
			    case -11:
			    case -10:
				    snprintf(buf_multiplier, sizeof(buf_multiplier), "n");
				    *value *= 1E+9;
				    break;

			    case -9:
			    case -8:
			    case -7:
				    snprintf(buf_multiplier, sizeof(buf_multiplier), "%s", uu);
				    *value *= 1E+6;
				    break;

			    case -6:
			    case -5:
			    case -4:
				    snprintf(buf_multiplier, sizeof(buf_multiplier), "m");
				    *value *= 1E+3;
				    break;

			    case -3:
			    case -2:
			    case -1:
				    snprintf(buf_multiplier, sizeof(buf_multiplier), " ");
				    break;

			    case 0:
			    case 1:
			    case 2:
				    snprintf(buf_multiplier, sizeof(buf_multiplier), " ");
				    break;

			    case 3:
			    case 4:
			    case 5:
				    snprintf(buf_multiplier, sizeof(buf_multiplier), "k");
				    *value /= 1E+3;
				    break;

			    case 6:
			    case 7:
			    case 8:
			    case 9:
				    snprintf(buf_multiplier, sizeof(buf_multiplier), "M");
				    *value /= 1E+6;
				    break;
		    }
		}
    }

    *units = *units;
}