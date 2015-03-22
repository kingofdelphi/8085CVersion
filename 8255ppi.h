#ifndef PPI_8255_H
#define PPI_8255_H
#include <stdio.h>
#include "sim8085.h"
#include "pthread.h"
#define MAX_PPI_CONNECTIONS ((256 >> 4) * 2) //since each ppi uses 4 address values
extern int address_latch, data_latch;
extern pthread_cond_t external_strobe_signal;
extern pthread_mutex_t external_strobe_signal_mutex;
typedef struct {
    Sim8085 * mpu; //simulator to which the ppi is connected to
    int chip_select;
    int control_register; //used to copy the control register value
    int thread_id;
    int porta, portb, portc, portd;
    int intr_a_signal, intr_b_signal, stb_a_signal, stb_b_signal;
    int intr_a_pin, intr_b_pin; //to which pin of MPU(trap, rst7.5, ..., rst5.5) is the intr_x pin connected to
    int inte_a, inte_b;//interrupt latches
    int port_a_input_latch, port_b_input_latch;
} ppi_8255;

//ppi will be connected to the cpu and then
//it will start operating automatically, each ppi on its own
//thread
void connect_ppi(ppi_8255 * ppi, Sim8085 * simulator, int chip_select, int inta_pin, int intb_pin);
//a = 0, b = 1, c = 2, control_register = 3
int get_ppi_port_value(ppi_8255 * ppi, int port_no); 
//cpu to ppi io r w handling is done by this function
void * ppi_mpu_thread_handler(void * t_info); //each ppi device has its own thread
//signal handling such as intr, stb is done by this handler
void * ppi_peripheral_thread_handler(void * t_info); //each ppi device has its own thread

void initialize_cpu_mutex();
#endif
