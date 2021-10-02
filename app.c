#include <gtk/gtk.h> // gui, unicode
#include <unistd.h>  // sleep, usleep
#include <pthread.h> // multithreading
#include <stdio.h>   // fprintf

#include <signal.h>   // signal, sigint
#include <stdint.h>   // uint
#include <sys/time.h> // gettimeofday
#include <float.h>    // DBL_MAX, DBL_MIN

#include "communication.h"

#define AVG_SAMPLES 10

#define REASON_SIGINT 0
#define REASON_GTK_EXIT 1

FILE *logfile;

GtkBuilder *builder;

GtkWidget *window;
GtkWidget *header;

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

void get_formated_datetime(char *strtimedate, int maxsize, const char *format, uint32_t *ms)
{
  int millisec;
  struct tm *tm_info;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  *ms = lrint(tv.tv_usec / 1000.0);
  if (*ms >= 1000)
  {
    *ms -= 1000;
    tv.tv_sec++;
  }
  tm_info = localtime(&tv.tv_sec);
  strftime(strtimedate, maxsize, format, tm_info);
}

void kill_me(uint8_t reason)
{
  g_print("\r\nUkončuji program, důvod: ");
  switch (reason)
  {
  case REASON_SIGINT:
    g_print("SIGINT");
    break;
  case REASON_GTK_EXIT:
    g_print("GUI");
    break;
  default:
    g_print("%d", reason);
  }
  g_print("\r\n");
  g_print("Ukončuji vlákno s aktualizací dat\r\n");
  uart_thread_running = 0;
  g_print("Ukončuji vlákno GTK\r\n");
  gtk_main_quit();
}

void on_sigint(int si)
{
  kill_me(REASON_SIGINT);
}

void on_destroy_wndmain(GtkWidget *widget, gpointer data)
{
  kill_me(REASON_GTK_EXIT);
}

void on_clicked_setup(GtkButton *button, gpointer data)
{
  g_print("\r\n!!! Nastavení zatím není implementované !!!\r\n");
}

void on_clicked_playpause(GtkButton *button, gpointer data)
{
  static uint8_t state;
  if (state == 0)
  {
    gtk_button_set_image(GTK_BUTTON(btnPlayPause), icoPlay);
    uart_logging = 0;
  }
  else if (state == 1)
  {
    gtk_button_set_image(GTK_BUTTON(btnPlayPause), icoPause);
    uart_logging = 1;
  }
  state = (state == 0) ? 1 : 0;
}

void *loop_uart(void *args)
{
  static uint32_t i;
  static double min = DBL_MAX;
  static double max = DBL_MIN;
  static double old;
  static double average;
  static double samples[AVG_SAMPLES];
  static uint16_t samples_count;

  while (uart_thread_running == 1)
  {
    if (uart_logging == 1)
    {
      gchar sfunction[MAX_LABEL_SIZE];
      gchar svalue[MAX_LABEL_SIZE];
      gchar smin[MAX_LABEL_SIZE];
      gchar smax[MAX_LABEL_SIZE];
      gchar savg[MAX_LABEL_SIZE];
      gchar sfile[MAX_LABEL_SIZE];
      gchar scounter[MAX_LABEL_SIZE];

      double value;
      uint8_t function;
      gchar units[MAX_LABEL_SIZE];
      multimeter_read(&function, &value, units);

      char dt[20];
      uint32_t ms;
      get_formated_datetime(dt, sizeof(dt), "%d.%m.%Y;%H:%M:%S", &ms);
      // Zapis do textoveho souboru
      fprintf(logfile, "%s.%03d;%.10f;%s;%s\r\n", dt, ms, value, units, fn_names[function]);
      g_print("%06d\t%.10f %s\t(%s)\r", i, value, units, fn_names[function]);

      samples[samples_count++] = value;

      if (samples_count == (AVG_SAMPLES))
      {
        float sumsamples = 0.0f;
        for (uint16_t si = 0; si < AVG_SAMPLES; si++)
        {
          sumsamples += samples[si];
        }
        average = sumsamples / (float)AVG_SAMPLES;
        samples_count = 0;
      }

      if (value > max)
        max = value;
      if (value < min)
        min = value;

      char prefix = '\0';

      if (value > 999999.0)
      {
        prefix = 'M';
        value /= 1000000.0f;
      }
      else if (value > 999.0)
      {
        prefix = 'k';
        value /= 1000.0f;
      }
      else if (value > 1.0)
      {
        prefix = 'm';
        value *= 1000.0f;
      }
      else if (value > 0.01)
      {
        prefix = 'u';
        value *= 1000000.0f;
      }
      else if (value > 0.00001)
      {
        prefix = 'n';
        value *= 1000000000.0f;
      }

      g_snprintf(sfunction, MAX_LABEL_SIZE, "%s (%c%s)", fn_names[function], prefix, units);
      if (prefix == '\0')
        g_snprintf(svalue, MAX_LABEL_SIZE, "%.10f", value);
      else
        g_snprintf(svalue, MAX_LABEL_SIZE, "%.3f", value);
      g_snprintf(smin, MAX_LABEL_SIZE, "%.6f", min);
      g_snprintf(smax, MAX_LABEL_SIZE, "%.6f", max);
      g_snprintf(savg, MAX_LABEL_SIZE, "%.6f", average);
      g_snprintf(sfile, MAX_LABEL_SIZE, "%s", config.logfilename);
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
    if (config.error_flag)
    {
      sleep(1);
    }
    else
    {
      usleep(config.interval);
    }
  }
}

int main(int argc, char **argv)
{
  signal(SIGINT, on_sigint);
  gtk_init(&argc, &argv);
  multimeter_communication_init(argc, argv);

  // Otevreni serioveho portu
  multimeter_open_port();

  // Dotaz na verzi/podpis multimetru
  // Overeni, ze funguje AT/SCPI komunikace
  gchar multimeter_info[MAX_LABEL_SIZE];
  multimeter_ping(multimeter_info);
  if (strlen(multimeter_info) > 0)
  {
    g_print("============================================\r\n");
    g_print("Detekoval jsem sériové SCPI zařízení:\r\n");
    g_print("%s\r\n", multimeter_info);
    g_print("============================================\r\n");
  }

  // Vytvoreni noveho logovaciho souboru
  // zaznam_yymmddhhiiss.csv
  uint32_t ms;
  get_formated_datetime(config.logfilename, sizeof(config.logfilename), "zaznam_%y%m%d%H%M%S.csv", &ms);
  logfile = fopen(config.logfilename, "w");

  // Stavime okno programu z externiho XML
  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "gui.glade", NULL);

  // Propojeni hlavniho okna
  window = GTK_WIDGET(gtk_builder_get_object(builder, "wndMain"));

  // Propojeni hlavicky okna
  header = GTK_WIDGET(gtk_builder_get_object(builder, "hbMain"));
  char subtitle[MAX_LABEL_SIZE];
  g_snprintf(subtitle, MAX_LABEL_SIZE, "Zařízení připojeno na %s", config.serial.device);
  gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), subtitle);

  // Propojeni labelu v hlavnim okne
  lblFunction = GTK_WIDGET(gtk_builder_get_object(builder, "lblFunction"));
  lblValue = GTK_WIDGET(gtk_builder_get_object(builder, "lblValue"));
  lblMin = GTK_WIDGET(gtk_builder_get_object(builder, "lblMin"));
  lblMax = GTK_WIDGET(gtk_builder_get_object(builder, "lblMax"));
  lblAvg = GTK_WIDGET(gtk_builder_get_object(builder, "lblAvg"));
  lblFile = GTK_WIDGET(gtk_builder_get_object(builder, "lblFile"));
  lblCounter = GTK_WIDGET(gtk_builder_get_object(builder, "lblCounter"));

  // Propojeni tlacitek v hlavnim okne
  btnPlayPause = GTK_WIDGET(gtk_builder_get_object(builder, "btnPlayPause"));

  // Propojeni ikon v hlavnim okne
  icoPlay = GTK_WIDGET(gtk_builder_get_object(builder, "icoPlay"));
  icoPause = GTK_WIDGET(gtk_builder_get_object(builder, "icoPause"));

  // Propojeni signalu
  gtk_builder_connect_signals(builder, NULL);

  // Zjev se, okno...
  gtk_widget_show(window);

  // Separatni vlakno pro aktualizaci dat z USB
  pthread_t th_uart;
  pthread_create(&th_uart, NULL, loop_uart, NULL);

  // Start GTK smycky
  gtk_main();

  // Ukonceni aktualizacniho vlakna
  pthread_join(th_uart, NULL);

  // Uzavreni logovaciho souboru
  fclose(logfile);

  // Uzavreni serioveho spojeni
  multimeter_close_port();

  // Rozlouceni
  g_print("Ukončuji vlákno aplikace\r\n");
  return 0;
}
