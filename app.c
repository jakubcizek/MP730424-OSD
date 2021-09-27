#include <gtk/gtk.h>
#include <unistd.h>  // sleep
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>

#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h> // lrint

#define FL __FILE__,__LINE__

#define SSIZE 1024

#define DATA_FRAME_SIZE 12
#define ee ""
#define uu "\u00B5"
#define kk "k"
#define MM "M"
#define mm "m"
#define nn "n"
#define pp "p"
#define dd "\u00B0"
#define oo "\u03A9"

#define MMFLAG_AUTORANGE	0b01000000

//#define MEAS_VOLT "MEAS:VOLT?"
//#define MEAS_CURR "MEAS:CURR?"
#define GET_FUNC "FUNC?"
#define MEAS_VALUE "MEAS?"

#define UI_FILE "gui.glade"
#define MAX_LABEL_SIZE 50
#define AVG_SAMPLES 10

FILE *logfile;
int i = 0;           // Generic counter
char temp_char;        // Temporary character
char tfn[4096];
int flag_ol = 0;
int ms = 0;
char *decimal_point;

struct serial_params_s {
	char *device;
	int fd, n;
	int cnt, size, s_cnt;
	struct termios oldtp, newtp;
};

typedef struct glb {
	uint8_t quiet;
	uint16_t flags;
	uint16_t error_flag;
	char *device;
	char get_func[20];
	char meas_value[20];
	char logfilename[30];
	char *com_address;
	char *serial_parameters_string; // this is the raw from the command line
	struct serial_params_s serial_params; // this is the decoded version
	int serial_timeout;
	int interval;
} glb;

glb g;

ssize_t data_write(glb *gl, char *d, ssize_t s );
ssize_t data_read(glb *gl, char *b, ssize_t s );

GtkWidget *window;
GtkBuilder *builder;

GtkWidget *lblFunction;
GtkWidget *lblValue;
GtkWidget *lblMin;
GtkWidget *lblMax;
GtkWidget *lblAvg;
GtkWidget *lblFile;
GtkWidget *lblCounter;

GtkWidget *btnPlayPause;
GtkWidget *icoPlay;
GtkWidget *icoPause;

uint8_t uart_thread_running = 1;
uint8_t uart_logging = 1;

char SEPARATOR_DP[] = ".";

void getFormatedDateTime(char *strtimedate, int maxsize, const char* format, int *ms){
  	int millisec;
	struct tm* tm_info;
  	struct timeval tv;
  	gettimeofday(&tv, NULL);
  	*ms = lrint(tv.tv_usec/1000.0);
  	if (*ms>=1000) {
    	*ms -=1000;
    	tv.tv_sec++;
  	}
  	tm_info = localtime(&tv.tv_sec);
  	strftime(strtimedate, maxsize, format, tm_info);
}

char digit( unsigned char dg ) {

	int d;
	char g;

	switch (dg) {
		case 0x30: g = '0'; d = 0; break;
		case 0x31: g = '1'; d = 1; break;
		case 0x32: g = '2'; d = 2; break;
		case 0x33: g = '3'; d = 3; break;
		case 0x34: g = '4'; d = 4; break;
		case 0x35: g = '5'; d = 5; break;
		case 0x36: g = '6'; d = 6; break;
		case 0x37: g = '7'; d = 7; break;
		case 0x38: g = '8'; d = 8; break;
		case 0x39: g = '9'; d = 9; break;
		case 0x3E: g = 'L'; d = 0; break;
		case 0x3F: g = ' '; d = 0; break;
		default: g = ' ';
	}

	return g;
}

int init(struct glb *g) {
	g->quiet = 0;
	g->flags = 0;
	g->error_flag = 0;
	g->interval = 100000;
	g->device = NULL;
	g->serial_timeout = 3;

	g->serial_parameters_string = NULL;

	return 0;
}

void open_port( struct glb *g ) {
	struct serial_params_s *s = &(g->serial_params);
	char *p = g->serial_parameters_string;
	char default_params[] = "115200:8n1";
	int r; 

	if (!p) p = default_params;

	g_print("Pokouším se otevřít %s ... ", s->device);
	s->fd = open( s->device, O_RDWR | O_NOCTTY | O_NDELAY );
	if (s->fd <0) {
		perror( s->device );
	}

	fcntl(s->fd,F_SETFL,0);
	tcgetattr(s->fd,&(s->oldtp)); // save current serial port settings 
	tcgetattr(s->fd,&(s->newtp)); // save current serial port settings in to what will be our new settings
	cfmakeraw(&(s->newtp));

	s->newtp.c_cflag = CS8 |  CLOCAL | CREAD ; 
	s->newtp.c_cflag |= B115200; 
	s->newtp.c_cc[VMIN] = 0;
	s->newtp.c_cc[VTIME] = g->serial_timeout *10; // VTIME is 1/10th's of second

	s->newtp.c_cflag &= ~(PARODD|PARENB);
	s->newtp.c_cflag &= ~CSTOPB;

	s->newtp.c_iflag &= ~(IXON | IXOFF | IXANY );

	r = tcsetattr(s->fd, TCSANOW, &(s->newtp));
	if (r) {
		g_print(" CHYBA!\r\n%s:%d: %s\r\n", FL, strerror(errno));
		exit(1);
	}

	g_print("OK (FD[%d])\r\n", s->fd);
}

uint8_t a2h( uint8_t a ) {
	a -= 0x30;
	if (a < 10) return a;
	a -= 7;
	return a;
}

ssize_t data_read( glb *gl, char *b, ssize_t s ) {
	ssize_t sz;
	*b = '\0';
	int bp = 0;
	do {
		char temp_char;
		sz = read(gl->serial_params.fd, &temp_char, 1);
		if (sz == -1) {
			b[0] = '\0';
			fprintf(stderr,"Nemohu precist data ze seriove linky - %s\n", strerror(errno));
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

ssize_t data_write( glb *gl, char *d, ssize_t s ) { 
	ssize_t sz;
	sz = write(gl->serial_params.fd, d, s); 
	if (sz < 0) {
		gl->error_flag = 1;
		fprintf(stdout,"Chyba v odesilani seriovych dat: %s\n", strerror(errno));
	}
	return sz;
}

void on_destroy_wndmain(GtkWidget* widget, gpointer data){
  g_print("\r\nUkončuji vlákno s aktualizací dat\r\n");
  uart_thread_running = 0;
  g_print("Ukončuji vlákno GTK\r\n");
  gtk_main_quit();
}

void on_clicked_playpause(GtkButton* button, gpointer data){
  static uint8_t state;
  if(state == 0){
    //gtk_button_set_label(GTK_BUTTON(btnPlayPause), "Obnovit");
    gtk_button_set_image(GTK_BUTTON(btnPlayPause), icoPlay);
    uart_logging = 0;
  }
  else if(state == 1){
    //gtk_button_set_label(GTK_BUTTON(btnPlayPause), "Pozastavit");
    gtk_button_set_image(GTK_BUTTON(btnPlayPause), icoPause);
    uart_logging = 1;
  }
  state = (state == 0)? 1:0;
}

void *loop_uart(void *args){
  static uint32_t i;
  static double min = 999999999.9999999;
  static double max = -999999999.9999999;
  static double old;
  static double average;
  static double samples[AVG_SAMPLES];
  static uint16_t samples_count;

  while(uart_thread_running == 1){
    if(uart_logging == 1){
      //g_print("%06d\tVolání z aktualizačního vlákna\r\n", i);
      gchar sfunction[MAX_LABEL_SIZE];
      gchar svalue[MAX_LABEL_SIZE];
      gchar smin[MAX_LABEL_SIZE];
      gchar smax[MAX_LABEL_SIZE];
      gchar savg[MAX_LABEL_SIZE];
      gchar sfile[MAX_LABEL_SIZE];
      gchar scounter[MAX_LABEL_SIZE];

      char line1[1024];
		  char line2[1024];
		  gchar buf_func[100];
		  char buf_value[100];
		  char buf_units[10];
		  char buf_multiplier[10];

		  char *p, *q;
		  double td = 0.0;
		  double v = 0.0;
		  int ev = 0;
		  uint8_t range;
		  uint8_t dpp = 0;
		  ssize_t bytes_read = 0;
		  ssize_t sz;

      sz = data_write( &g, g.get_func, strlen(g.get_func)  );
		  if (sz > -1) {
			  sz = data_read( &g, buf_func, sizeof(buf_func) );
			  if (sz > -1) {
				  if (strstr(buf_func,"E+") || strstr(buf_func,"E-")) sz = data_read( &g, buf_func, sizeof(buf_func) );
			  } else { 
				  snprintf(buf_func,sizeof(buf_func),"ZADNA DATA");
			  }
		  } else { 
			  snprintf(buf_func,sizeof(buf_func),"ZADNA DATA");
		  }

		  sz = data_write( &g, g.meas_value, strlen(g.meas_value));
		  if (sz > -1) {
			  sz = data_read( &g, buf_value, sizeof(buf_value));
			  if (sz > -1) {
			  } else {
				  snprintf(buf_value,sizeof(buf_value),"ZADNA DATA");
			  }
		  } else {
			  snprintf(buf_value,sizeof(buf_value),"ZADNA DATA");
		  }

      if ( strlen( buf_func ) == 0 || strlen( buf_value ) == 0 ) {
			  snprintf(line1, sizeof(line1), "ZADNA DATA");
			  snprintf(line2, sizeof(line2), "* * * *");
		  }

		  else {
			  if (strncmp(buf_value, "1E+9", 4)==0) {
				  snprintf(line1,sizeof(line1),"Pretizeni");
				  flag_ol = 1;
			  } else {
				  char *e;
				  flag_ol = 0;
          // Local decimal point replacement
          char *pos = strchr(buf_value, '.');
          if(pos) *pos = *decimal_point;
				  td = strtod(buf_value,NULL);
				  snprintf(line1, sizeof(line1), "%f %s", td, buf_units);
				  e = strstr(buf_value,"E+");
				  if (!e) e = strstr(buf_value, "E-");
				  if (e) {
					  ev = strtol(e+2, NULL, 10);
				  }
			  }
		  }


		  if (strncmp("\"DIOD",buf_func, 5)==0) {
			  //
			  // Work around for firmware fault
			  //
			  g_snprintf(buf_func,sizeof(buf_func), "Kontinuita");
			  snprintf(buf_units,sizeof(buf_units) ,"%s", oo);
		  } 
		  else if (strncmp("\"CONT",buf_func, 5)==0) {
			  //
			  // Work around for firmware fault
			  //
			  g_snprintf(buf_func,sizeof(buf_func), "Dioda");
			  snprintf(buf_units,sizeof(buf_units) ,"V");
		  } 
		  else if (strncmp("\"VOLT AC\"",buf_func, 6)==0) {
			  g_snprintf(buf_func,sizeof(buf_func), "Střídavé napětí");
			  snprintf(buf_units,sizeof(buf_units) ,"V");
		  } 
		  else if (strncmp("\"VOLT\"",buf_func, 6)==0) {
			  g_snprintf(buf_func,sizeof(buf_func), "Stejnosměrné napětí");
			  snprintf(buf_units,sizeof(buf_units) ,"V");
		  } 
		  else if (strncmp("\"CURR",buf_func, 5)==0) {
			  g_snprintf(buf_func,sizeof(buf_func), "Proud");
			  snprintf(buf_units,sizeof(buf_units) ,"A");
		  } 
		  else if (strncmp("\"RES",buf_func, 4)==0) {
			  g_snprintf(buf_func,sizeof(buf_func), "Odpor");
			  snprintf(buf_units,sizeof(buf_units) ,"%s", oo);
		  } 
		  else if (strncmp("\"CAP",buf_func, 4)==0) {
			  g_snprintf(buf_func,sizeof(buf_func), "Kapacita");
			  snprintf(buf_units,sizeof(buf_units) ,"F");
		  } 
		  else if (strncmp("\"FREQ",buf_func, 5)==0) {
			  g_snprintf(buf_func,sizeof(buf_func), "Frekvence");
			  snprintf(buf_units,sizeof(buf_units) ,"Hz");
		  } 
		  else if (strncmp("\"TEMP",buf_func, 5)==0) {
			  g_snprintf(buf_func,sizeof(buf_func), "Teplota");
			  snprintf(buf_units,sizeof(buf_units) ,"%sC", dd);
		  } 

		  if (flag_ol == 1) snprintf(line1, sizeof(line1), "OL");

		  if (flag_ol == 0) {
		  switch (ev) {
			  case -12:
			  case -11:
			  case -10:
				  snprintf(buf_multiplier, sizeof(buf_multiplier), "n");
				  td *= 1E+9;
				  snprintf(line1, sizeof(line1), "%0.3f%s%s", td, buf_multiplier, buf_units);
				  break;

			  case -9:
			  case -8:
			  case -7:
				  snprintf(buf_multiplier, sizeof(buf_multiplier), "%s", uu);
				  td *= 1E+6;
				  snprintf(line1, sizeof(line1), "%0.3f%s%s", td, buf_multiplier, buf_units);
				  break;

			  case -6:
			  case -5:
			  case -4:
				  snprintf(buf_multiplier, sizeof(buf_multiplier), "m");
				  td *= 1E+3;
				  snprintf(line1, sizeof(line1), "%0.3f%s%s", td, buf_multiplier, buf_units);
				  break;

			  case -3:
			  case -2:
			  case -1:
				  snprintf(buf_multiplier, sizeof(buf_multiplier), " ");
				  snprintf(line1, sizeof(line1), "%0.3f%s%s", td, buf_multiplier, buf_units);
				  //					td *= 1E+6;
				  break;

			  case 0:
			  case 1:
			  case 2:
				  snprintf(buf_multiplier, sizeof(buf_multiplier), " ");
				  //					td *= 1E+6;
				  snprintf(line1, sizeof(line1), "%0.3f%s%s", td, buf_multiplier, buf_units);
				  break;

			  case 3:
			  case 4:
			  case 5:
				  snprintf(buf_multiplier, sizeof(buf_multiplier), "k");
				  td /= 1E+3;
				  snprintf(line1, sizeof(line1), "%0.3f%s%s", td, buf_multiplier, buf_units);
				  break;

			  case 6:
			  case 7:
			  case 8:
			  case 9:
				  snprintf(buf_multiplier, sizeof(buf_multiplier), "M");
				  td /= 1E+6;
				  snprintf(line1, sizeof(line1), "%0.3f%s%s", td, buf_multiplier, buf_units);
				  break;
		    }
		  }

      char dt[20];
  		getFormatedDateTime(dt, sizeof(dt), "%d.%m.%Y;%H:%M:%S", &ms);
			// Zapis do textoveho souboru
			fprintf(logfile, "%s.%03d;%s;%f;\"%s\";%s;%s\r\n", dt, ms, buf_func, td, line1, buf_multiplier, buf_units);
      g_print("%06d\t%s\t%f %s\r", i, buf_func, td, buf_units);


      samples[samples_count++] = td;

      if(samples_count == (AVG_SAMPLES)){
        float sumsamples = 0.0f;
        for(uint16_t si = 0; si < AVG_SAMPLES; si++){
          sumsamples += samples[si];
        }
        average = sumsamples / (float)AVG_SAMPLES;
        samples_count = 0;
      }

      if(td > max) max = td;
      if(td < min) min = td;

      g_snprintf(sfunction, MAX_LABEL_SIZE, "%s (%s)", buf_func, buf_units);
      g_snprintf(svalue, MAX_LABEL_SIZE, "%.6f", td);
      g_snprintf(smin, MAX_LABEL_SIZE, "%.6f", min);
      g_snprintf(smax, MAX_LABEL_SIZE, "%.6f", max);
      g_snprintf(savg, MAX_LABEL_SIZE, "%.6f", average);
      g_snprintf(sfile, MAX_LABEL_SIZE, "%s", g.logfilename);
      g_snprintf(scounter, MAX_LABEL_SIZE, "# %06d", i);
      gtk_label_set_text(GTK_LABEL(lblFunction), sfunction);
      gtk_label_set_text(GTK_LABEL(lblValue), svalue);
      gtk_label_set_text(GTK_LABEL(lblMin), smin);
      gtk_label_set_text(GTK_LABEL(lblMax), smax);
      gtk_label_set_text(GTK_LABEL(lblAvg), savg);
      gtk_label_set_text(GTK_LABEL(lblFile), sfile);
      gtk_label_set_text(GTK_LABEL(lblCounter), scounter);
      i++;
    }
    if (g.error_flag) {
			sleep(1);
		} else {
			usleep(g.interval);
		}
  }
}

int main(int argc, char **argv)
{
  init(&g);
  gtk_init (&argc, &argv);

  if(argc == 2) g.device = argv[1];
  else{
    printf("Uved cestu k seriovemu zarizeni multimetru (/dev/ttyUSB0 aj.)");
    return 0;
  }

	g.serial_params.device = g.device;
	snprintf(g.meas_value,sizeof(g.meas_value), "%s\n", MEAS_VALUE);
	snprintf(g.get_func,sizeof(g.get_func), "%s\n", GET_FUNC);

  open_port( &g );

  getFormatedDateTime(g.logfilename, sizeof(g.logfilename), "zaznam_%y%m%d%H%M%S.csv", &ms);
	logfile = fopen(g.logfilename, "w");


  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, UI_FILE, NULL);

  window = GTK_WIDGET (gtk_builder_get_object (builder, "wndMain"));

  lblFunction = GTK_WIDGET(gtk_builder_get_object (builder, "lblFunction"));
  lblValue = GTK_WIDGET(gtk_builder_get_object (builder, "lblValue"));
  lblMin = GTK_WIDGET(gtk_builder_get_object (builder, "lblMin"));
  lblMax = GTK_WIDGET(gtk_builder_get_object (builder, "lblMax"));
  lblAvg = GTK_WIDGET(gtk_builder_get_object (builder, "lblAvg"));
  lblFile = GTK_WIDGET(gtk_builder_get_object (builder, "lblFile"));
  lblCounter = GTK_WIDGET(gtk_builder_get_object (builder, "lblCounter"));

  btnPlayPause = GTK_WIDGET(gtk_builder_get_object (builder, "btnPlayPause"));
  icoPlay = GTK_WIDGET(gtk_builder_get_object (builder, "icoPlay"));
  icoPause = GTK_WIDGET(gtk_builder_get_object (builder, "icoPause"));

  gtk_builder_connect_signals (builder, NULL);

  gtk_widget_show (window);

  struct lconv *lc;
  lc = localeconv();
  decimal_point = lc->decimal_point;

  pthread_t th_uart;
  pthread_create(&th_uart, NULL, loop_uart, NULL);

  gtk_main();
  pthread_join(th_uart, NULL);

  // Implementovat: Zavrit soubor
  fclose(logfile);

  g_print("Ukončuji vlákno aplikace\r\n");
  return 0;
}