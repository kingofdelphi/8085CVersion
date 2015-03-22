#ifndef PPI_8255_H
#define PPI_8255_H
#include <stdio.h>
#include "sim8085.h"
#include "pthread.h"
#define MAX_PPI_CONNECTIONS (256 >> 4) //since each ppi uses 4 address values
typedef struct {
    Sim8085 * mpu; //simulator to which the ppi is connected to
    int chip_select;
    int control_register; //used to copy the control register value
    int thread_id;
    int porta, portb, portc, portd;
} ppi_8255;

//ppi will be connected to the cpu and then
//it will start operating automatically, each ppi on its own
//thread
void connect_ppi(ppi_8255 * ppi, Sim8085 * simulator, int base_address);
//a = 0, b = 1, c = 2, control_register = 3
int get_ppi_port_value(ppi_8255 * ppi, int port_no); 

void * ppi_thread_handler(void * t_info); //each ppi device has its own thread

void initialize_cpu_mutex();
#endif
