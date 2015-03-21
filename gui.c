#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "sim8085.h"
#include "8255ppi.h"
//program buffer and path
char * program = 0;
char programpath[255];
Sim8085 simulator;
int program_loaded = 0;
int program_execution = 0; //if simulator is in execution i.e. not in single step, used to identify if a separate thread
                             //is being run
pthread_mutex_t sim_mutex, broadcast_mutex;
pthread_cond_t cpu_started = PTHREAD_COND_INITIALIZER;//to signal other devices if cpu has been started
GtkWidget *window, *grid, *openBtn, * quitBtn, *toolbar, *view, * errorview;
GtkTextBuffer *buffer;
GtkTextBuffer *errorlog;
//flags
GtkWidget * signlbl, * zerolbl, * auxlbl, * paritylbl, * carrylbl;
//registers
GtkWidget * rega, * regb, * regc, * regd, * rege, * regh, * regl, * regsp, * regpc;
//text color tags
GtkTextTag * labeltag, * instructiontag, * defaulttag, * registertag, * commenttag, * currentlinetag;

//memory, io access
GtkWidget * inputaddress;
GtkWidget * addressval;
GtkWidget * readfromedit;
//status bar

void loadfilefrompath(const char * filename) {
    gchar *text;
    gsize length;
    GError *error = NULL;
    if (g_file_get_contents (filename, &text, &length, NULL)) {
        program = g_convert (
                text, length, "UTF-8", "ISO-8859-1", NULL, NULL, &error);
    }
}
const char * identifiers[] = {
    "ADC", "ADD", "ANA", "CMA", "CMC",
    "CMP", "DAA", "DAD", "DCR", "DCX",
    "HLT", "INR", "INX", "LDAX",
    "MOV",  "NOP", "ORA", "PCHL",
    "POP", "PUSH", "RAL", "RAR", "RC",
    "RET", "RLC", "RM", "RNC", "RNZ", "RP",
    "RPE", "RPO", "RRC", "RZ", "SBB", "SPHL",
    "STAX", "STC", "SUB", "XCHG", "XRA", "XTHL",

    "ACI", "ADI", "ANI", "CPI", "IN", "MVI",
    "ORI", "OUT", "SBI", "SUI", "XRI", 

    "CALL", "CC", "CM", "CNC", "CNZ", "CP",
    "CPE", "CPO", "CZ", "JC", "JM", "JMP",
    "JNC", "JNZ", "JP", "JPE", "JPO", "JZ",
    "LDA", "LHLD", "LXI", "SHLD", "STA",

    "CALL", "CC", "CM", "CNC", "CNZ", "CP",
    "CPE", "CPO", "CZ", "JC", "JM", "JMP",
    "JNC", "JNZ", "JP", "JPE", "JPO", "JZ",
};

const char * registers[] = { "A", "B", "C", "D", "E", "H", "L", "SP" };
int isregister(const char * s, int n) {
    for (int i = 0; i < sizeof(registers) / sizeof(*registers); ++i) {
        if (NSTRCMP(registers[i], s, n) == 0) return 1;
    }
    return 0;
}
int isinstruction(const char * s, int n) {
    for (int i = 0; i < sizeof(identifiers) / sizeof(*identifiers); ++i) {
        if (NSTRCMP(identifiers[i], s, n) == 0) return 1;
    }
    return 0;
}
int islabel(const char * s, int n) {
    return n > 0 && s[n - 1] == ':';
}
int iscomment(const char * s, int n) {
    return n > 0 && *s == '$';
}

void cleartextbuffer() {
    gtk_text_buffer_set_text(buffer, "", -1);
}
void reloadtextbuffer() {
    gtk_text_buffer_set_text(buffer, program, -1);
}
void freeprogram() {
    if (program) {
        free(program);
    }
}

void recolorBuffer() {
    int lines = gtk_text_buffer_get_line_count(buffer);
    GtkTextIter is, ie;
    gtk_text_buffer_get_start_iter(buffer, &is);
    gtk_text_buffer_get_end_iter(buffer, &ie);
    gtk_text_buffer_remove_tag(buffer, instructiontag, &is, &ie);
    gtk_text_buffer_remove_tag(buffer, labeltag, &is, &ie);
    gtk_text_buffer_remove_tag(buffer, registertag, &is, &ie);
    gtk_text_buffer_remove_tag(buffer, commenttag, &is, &ie);
    gtk_text_buffer_remove_tag(buffer, currentlinetag, &is, &ie);
    for (int i = 0; i < lines; ++i) {
        GtkTextIter is, ie;
        gtk_text_buffer_get_iter_at_line(buffer, &is, i);
        gtk_text_buffer_get_iter_at_line(buffer, &ie, i);
        if (!gtk_text_iter_ends_line(&ie)) gtk_text_iter_forward_to_line_end(&ie);
        pthread_mutex_lock(&sim_mutex); //lock simulator access
        if (program_loaded && !hasHalted(&simulator) && i == getIL(&simulator)) {
            gtk_text_buffer_apply_tag(buffer, currentlinetag, &is, &ie);
        }
        pthread_mutex_unlock(&sim_mutex);
        //g_print("line %d  |%s|\n", i, (const char *)txt);
        gchar * txt = gtk_text_buffer_get_text(buffer, &is, &ie, 0);
        int n = strlen(txt);  //discard newline, need to look it later
        int current = 0, toks = 0, toke = 0;
        while (current < n) {
            gettoken(txt, &current, &toks, &toke);
            GtkTextIter tstart, tend;
            //g_print("line %d toks %d toke %d\n", i, toks, toke);
            gtk_text_buffer_get_iter_at_line_offset(buffer, &tstart, i, toks);
            gtk_text_buffer_get_iter_at_line_offset(buffer, &tend, i, toke);
            if (islabel(txt + toks, toke - toks)) {
                gtk_text_buffer_apply_tag (buffer, labeltag, &tstart, &tend);
            } else if (isinstruction(txt + toks, toke - toks)) {
                gtk_text_buffer_apply_tag (buffer, instructiontag, &tstart, &tend);
            } else if (isregister(txt + toks, toke - toks)) {
                gtk_text_buffer_apply_tag (buffer, registertag, &tstart, &tend);
            } else if (iscomment(txt + toks, toke - toks)) {
                if (!gtk_text_iter_ends_line(&tend)) gtk_text_iter_forward_to_line_end(&tend);
                gtk_text_buffer_apply_tag (buffer, commenttag, &tstart, &tend);
            }
        }
        g_free(txt);
    }
}

void openfiledialog() {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new ("Open File",
            GTK_WINDOW(window),
            action,
            "_Cancel",
            GTK_RESPONSE_CANCEL,
            "_Open",
            GTK_RESPONSE_ACCEPT,
            NULL);

    res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        filename = gtk_file_chooser_get_filename (chooser);
        gtk_text_buffer_set_text(buffer, filename, -1);
        strcpy(programpath, filename);
        loadfilefrompath(programpath);
        reloadtextbuffer();
        g_free (filename);
    }

    gtk_widget_destroy (dialog);
}
void createinfolabels() {
    signlbl = gtk_label_new("0");
    zerolbl = gtk_label_new("0");
    auxlbl = gtk_label_new("0");
    paritylbl = gtk_label_new("0");
    carrylbl = gtk_label_new("0");

    rega = gtk_label_new("0");
    regb = gtk_label_new("0");
    regc = gtk_label_new("0");
    regd = gtk_label_new("0");
    rege = gtk_label_new("0");
    regh = gtk_label_new("0");
    regl = gtk_label_new("0");

    regsp = gtk_label_new("0");
    regpc = gtk_label_new("0");
    //text labels
    inputaddress = gtk_entry_new();
    addressval = gtk_entry_new();
    readfromedit = gtk_entry_new();

    //status bar items, error logs
    errorview = gtk_text_view_new ();
    errorlog = gtk_text_view_get_buffer (GTK_TEXT_VIEW (errorview)); 

    view = gtk_text_view_new ();
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)); 

    labeltag = gtk_text_buffer_create_tag (buffer, 
            "labeltag", "foreground", "red", NULL);  
    instructiontag = gtk_text_buffer_create_tag (buffer, 
            "instructiontag", "foreground", "green", NULL);  
    defaulttag = gtk_text_buffer_create_tag (buffer, 
            "defaulttag", "foreground", "black", NULL);  
    registertag = gtk_text_buffer_create_tag (buffer, 
            "registertag", "foreground", "brown", NULL);  
    commenttag = gtk_text_buffer_create_tag (buffer, 
            "commenttag", "foreground", "blue", NULL);  
    currentlinetag = gtk_text_buffer_create_tag (buffer, 
            "currentlinetag", "background", "gray", NULL);  
}
void changed() {
    recolorBuffer();
    program_loaded = 0; //reload simulator when the buffer changes
}
//program information
void showstatusbarinfo() {
    recolorBuffer();
}
void stopsimulator() {
    program_loaded = 0;
    program_execution = 0; //to stop the main thread
    recolorBuffer();
}
void updatempuinfo() {
    char tmp[255];
    //register update
    sprintf(tmp, "%d", simulator.REGISTER[0] & 0xFF);
    gtk_label_set_text(GTK_LABEL(rega), tmp);
    sprintf(tmp, "%d", simulator.REGISTER[1] & 0xFF);
    gtk_label_set_text(GTK_LABEL(regb), tmp);
    sprintf(tmp, "%d", simulator.REGISTER[2] & 0xFF);
    gtk_label_set_text(GTK_LABEL(regc), tmp);
    sprintf(tmp, "%d", simulator.REGISTER[3] & 0xFF);
    gtk_label_set_text(GTK_LABEL(regd), tmp);
    sprintf(tmp, "%d", simulator.REGISTER[4] & 0xFF);
    gtk_label_set_text(GTK_LABEL(rege), tmp);
    sprintf(tmp, "%d", simulator.REGISTER[5] & 0xFF);
    gtk_label_set_text(GTK_LABEL(regh), tmp);
    sprintf(tmp, "%d", simulator.REGISTER[6] & 0xFF);
    gtk_label_set_text(GTK_LABEL(regl), tmp);
    //flags update
    sprintf(tmp, "%d", getsign(&simulator));
    gtk_label_set_text(GTK_LABEL(signlbl), tmp);
    sprintf(tmp, "%d", getzero(&simulator));
    gtk_label_set_text(GTK_LABEL(zerolbl), tmp);
    sprintf(tmp, "%d", getAF(&simulator));
    gtk_label_set_text(GTK_LABEL(auxlbl), tmp);
    sprintf(tmp, "%d", getparity(&simulator));
    gtk_label_set_text(GTK_LABEL(paritylbl), tmp);
    sprintf(tmp, "%d", getcarry(&simulator));
    gtk_label_set_text(GTK_LABEL(carrylbl), tmp);
    //pc update
    sprintf(tmp, "%d", simulator.SP);
    gtk_label_set_text(GTK_LABEL(regsp), tmp);
    sprintf(tmp, "%d", simulator.PC);
    gtk_label_set_text(GTK_LABEL(regpc), tmp);
}
void simulatorstartcheck() {
    if (hasHalted(&simulator)) {
        program_loaded = 0;
    }
    if (!program_loaded) {
        ProgramFile lines;
        lines.count = 0; //fuck, forgot to initialize it :P
        int linecount = gtk_text_buffer_get_line_count(buffer);
        for (int i = 0; i < linecount; ++i) {
            GtkTextIter is, ie;
            gtk_text_buffer_get_iter_at_line(buffer, &is, i);
            gtk_text_buffer_get_iter_at_line(buffer, &ie, i);
            if (!gtk_text_iter_ends_line(&ie)) gtk_text_iter_forward_to_line_end(&ie);
            gchar * txt = gtk_text_buffer_get_text(buffer, &is, &ie, 0);
            strcpy(lines.lines[lines.count], txt);
            g_free(txt);
            lines.count++;
        }
        loadprogram(&simulator, 5000, &lines);
        if (simulator.err_list.count || simulator.byte_list.count == 0) {
            int msglen = 1 + simulator.err_list.count; //there are err_list.count newlines too 
            if (simulator.err_list.count) {
                for (int i = 0; i < simulator.err_list.count; ++i) {
                    msglen += strlen(simulator.err_list.err_list[i]);
                }
                char * errorstr = (char*)malloc(sizeof(char) * msglen);
                *errorstr = '\0';
                for (int i = 0; i < simulator.err_list.count; ++i) {
                    char tmps[255];
                    sprintf(tmps, "%s\n", simulator.err_list.err_list[i]);
                    strcat(errorstr, tmps);
                }
                gtk_text_buffer_set_text(errorlog, errorstr, -1);
                free(errorstr);
            } else {
                gtk_text_buffer_set_text(errorlog, "Error: Empty program", -1);
            }
        } else {
            program_loaded = 1;
            gtk_text_buffer_set_text(errorlog, "Compile successful\n", -1);
        }
    }
}
void runlogic() {
    simulatorstartcheck();
}
void infoupdate() {
    updatempuinfo();
    showstatusbarinfo();
    pthread_mutex_lock(&sim_mutex);
    if (program_loaded && !hasHalted(&simulator)) {
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_line(buffer, &iter, getIL(&simulator));
        gtk_text_view_scroll_to_iter(view, &iter, 0, 1, 0.5, 0.7);
    }
    pthread_mutex_unlock(&sim_mutex);
}
gint idleupdate(gpointer data) {
    infoupdate();
    return 1;
}
void singlesteplogic() {
    if (program_execution && !hasHalted(&simulator)) {
        gtk_text_buffer_set_text(errorlog, "Error: simulator is already running", -1);
        return ;
    }
    int pl = program_loaded && !hasHalted(&simulator); //check if simulator has already been run
    simulatorstartcheck();
    if (!program_loaded) return ;
    if (pl) {//if simulator was not previously running then do not run single step
        //in order to highlight the start line(line 0)
        if (!hasHalted(&simulator)) { 
            singlestep(&simulator);
            pthread_mutex_lock(&broadcast_mutex);
            pthread_cond_broadcast(&cpu_started);
            pthread_mutex_unlock(&broadcast_mutex);
        } else {
            program_loaded = 0;
        }
    }
    infoupdate();
}

//do not touch the gui from this function
//which will be running on a separate thread
void executestep(void * arg) {
    program_execution = 1; //signal that thread loop is running
    while (program_execution && !hasHalted(&simulator)) {
        pthread_mutex_lock(&sim_mutex); //lock before touching the simulator, there is another thread querying mpu status
        singlestep(&simulator);
        pthread_mutex_unlock(&sim_mutex);
        pthread_mutex_lock(&broadcast_mutex);
        pthread_cond_broadcast(&cpu_started);
        pthread_mutex_unlock(&broadcast_mutex);
    }
    program_execution = 0;
    pthread_exit((void*)0);
}
pthread_t thread;
//if mpu was in single step mode, we can continue executing
void executelogic() {
    if (program_execution) {
        gtk_text_buffer_set_text(errorlog, "Error: simulator is already running", -1);
    } else {
        simulatorstartcheck();
        if (program_execution == 0 && program_loaded) {
            pthread_create(&thread, 0, executestep, 0);
        }
    }
}
void inttostr(int no, char * dest) {
    int t = no;
    int c = no == 0;
    while (no) {
        no /= 10;
        c++;
    }
    dest[c] = 0;
    while (c) {
        dest[--c] = t % 10 + '0';
        t /= 10;
    }
}
int strtoint(const char * s) {
    int no = 0;
    for (int i = 0; i < strlen(s); ++i) {
        no = no * 10 + s[i] - '0';
    }
    return no;
}
void readfrommem() {
    const char * txt = gtk_entry_get_text(readfromedit);
    int no = strtoint(txt);
    no = simulator.RAM[no];
    char tmp[255];
    inttostr(no, tmp);
    gtk_entry_set_text(readfromedit, tmp);
}
void readfromio() {
    const char * txt = gtk_entry_get_text(readfromedit);
    int no = strtoint(txt);
    no = simulator.IOPORTS[no];
    char tmp[255];
    inttostr(no, tmp);
    gtk_entry_set_text(readfromedit, tmp);
}
void writetomem() {
    const char * txt = gtk_entry_get_text(inputaddress);
    int address = strtoint(txt);
    txt = gtk_entry_get_text(addressval);
    int value = strtoint(txt);
    simulator.RAM[address] = value;
}
void writetoio() {
    const char * txt = gtk_entry_get_text(inputaddress);
    int address = strtoint(txt);
    txt = gtk_entry_get_text(addressval);
    int value = strtoint(txt);
    simulator.IOPORTS[address] = value;
}
int main (int   argc, char *argv[]) {
    gtk_init (&argc, &argv);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title (GTK_WINDOW (window), "8085 Simulator");

    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);

    createinfolabels();
    g_signal_connect(buffer, "changed", G_CALLBACK(changed), 0);
    gtk_text_buffer_set_text (buffer, "", -1);

    GtkWidget* navbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(navbar), GTK_TOOLBAR_BOTH);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(navbar), GTK_ICON_SIZE_LARGE_TOOLBAR);
    GtkToolItem* tbtn = gtk_tool_button_new(gtk_image_new_from_file("icons/new.ico"), "New");
    g_signal_connect(tbtn, "clicked", G_CALLBACK(cleartextbuffer), 0);
    gtk_toolbar_insert(GTK_TOOLBAR(navbar), tbtn, 0);
    tbtn = gtk_tool_button_new(gtk_image_new_from_file("icons/open.ico"), "Open");
    g_signal_connect(tbtn, "clicked", G_CALLBACK(openfiledialog), 0);

    gtk_toolbar_insert(GTK_TOOLBAR(navbar), tbtn, 1);
    tbtn = gtk_tool_button_new(gtk_image_new_from_file("icons/run.ico"), "Run");
    g_signal_connect(tbtn, "clicked", G_CALLBACK(executelogic), 0);
    gtk_toolbar_insert(GTK_TOOLBAR(navbar), tbtn, 2);
    tbtn = gtk_tool_button_new(gtk_image_new_from_file("icons/run.ico"), "Step");
    gtk_toolbar_insert(GTK_TOOLBAR(navbar), tbtn, 3);
    g_signal_connect(tbtn, "clicked", G_CALLBACK(singlesteplogic), 0);
    tbtn = gtk_tool_button_new(gtk_image_new_from_file("icons/stop.ico"), "Stop");
    gtk_toolbar_insert(GTK_TOOLBAR(navbar), tbtn, 4);
    g_signal_connect(tbtn, "clicked", G_CALLBACK(stopsimulator), 0);

    //start placing objects onto the window
    GtkWidget * mainbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget * menubox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * editor_scroll = gtk_scrolled_window_new(0, 0);
    GtkWidget * errorlogscroll = gtk_scrolled_window_new(0, 0);
    GtkWidget * infobox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget * corebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * flagbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * reghorz1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * reghorz2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * reghorz3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * reghorz4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * spbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * pcbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * statusbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * memportbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * buttonbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * buttonbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_container_add(GTK_CONTAINER(window), mainbox);
    gtk_container_add(GTK_CONTAINER(mainbox), menubox);
    gtk_container_add(GTK_CONTAINER(mainbox), corebox);
    gtk_container_add(GTK_CONTAINER(corebox), editor_scroll);
    gtk_container_add(GTK_CONTAINER(corebox), infobox);
    gtk_container_add(GTK_CONTAINER(mainbox), statusbox);

    GtkWidget * label = gtk_label_new("FLAGS");
    gtk_box_pack_start(GTK_BOX(infobox), label, 0, 0, 0);

    gtk_container_add(GTK_CONTAINER(infobox), flagbox);

    label = gtk_label_new("REGISTERS");
    gtk_box_pack_start(GTK_BOX(infobox), label, 0, 0, 1);


    gtk_container_add(GTK_CONTAINER(infobox), reghorz1);
    gtk_container_add(GTK_CONTAINER(infobox), reghorz2);
    gtk_container_add(GTK_CONTAINER(infobox), reghorz3);
    gtk_container_add(GTK_CONTAINER(infobox), reghorz4);

    label = gtk_label_new("PSTAT");
    gtk_box_pack_start(GTK_BOX(infobox), label, 0, 0, 1);

    gtk_container_add(GTK_CONTAINER(infobox), spbox);
    gtk_container_add(GTK_CONTAINER(infobox), pcbox);
    gtk_container_add(GTK_CONTAINER(infobox), memportbox);
    gtk_container_add(GTK_CONTAINER(infobox), buttonbox1);

    gtk_box_pack_start(GTK_BOX(infobox), inputaddress, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(infobox), addressval, 0, 0, 1);

    gtk_container_add(GTK_CONTAINER(infobox), buttonbox2);

    gtk_box_pack_start(GTK_BOX(menubox), navbar, 1, 1, 1);

    label = gtk_label_new("S");
    gtk_box_pack_start(GTK_BOX(flagbox), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(flagbox), signlbl, 0, 0, 1);

    label = gtk_label_new("Z");
    gtk_box_pack_start(GTK_BOX(flagbox), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(flagbox), zerolbl, 0, 0, 1);

    label = gtk_label_new("AC");
    gtk_box_pack_start(GTK_BOX(flagbox), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(flagbox), auxlbl, 0, 0, 1);

    label = gtk_label_new("P");
    gtk_box_pack_start(GTK_BOX(flagbox), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(flagbox), paritylbl, 0, 0, 1);

    label = gtk_label_new("C");
    gtk_box_pack_start(GTK_BOX(flagbox), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(flagbox), carrylbl, 0, 0, 1);


    label = gtk_label_new("A");
    gtk_box_pack_start(GTK_BOX(reghorz1), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(reghorz1), rega, 0, 0, 1);

    label = gtk_label_new("B");
    gtk_box_pack_start(GTK_BOX(reghorz2), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(reghorz2), regb, 0, 0, 1);

    label = gtk_label_new("C"); 
    gtk_box_pack_start(GTK_BOX(reghorz2), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(reghorz2), regc, 0, 0, 1);

    label = gtk_label_new("D");
    gtk_box_pack_start(GTK_BOX(reghorz3), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(reghorz3), regd, 0, 0, 1);

    label = gtk_label_new("E");
    gtk_box_pack_start(GTK_BOX(reghorz3), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(reghorz3), rege, 0, 0, 1);

    label = gtk_label_new("H");
    gtk_box_pack_start(GTK_BOX(reghorz4), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(reghorz4), regh, 0, 0, 1);

    label = gtk_label_new("L");
    gtk_box_pack_start(GTK_BOX(reghorz4), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(reghorz4), regl, 0, 0, 1);

    label = gtk_label_new("SP");
    gtk_box_pack_start(GTK_BOX(spbox), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(spbox), regsp, 0, 0, 1);

    label = gtk_label_new("PC");
    gtk_box_pack_start(GTK_BOX(pcbox), label, 0, 0, 1);
    gtk_box_pack_start(GTK_BOX(pcbox), regpc, 0, 0, 1);

    gtk_container_add(GTK_CONTAINER(editor_scroll), view);
    
    //status bar items
    gtk_box_pack_start(GTK_BOX(statusbox), errorlogscroll, 1, 1, 1);
    gtk_container_add(GTK_CONTAINER(errorlogscroll), errorview);

    //memory / port access
    gtk_box_pack_start(GTK_BOX(memportbox), readfromedit, 0, 0, 1);
    GtkWidget * btn = gtk_button_new_with_label("From IO");
    gtk_box_pack_start(GTK_BOX(buttonbox1), btn, 0, 0, 1);
    g_signal_connect(btn, "clicked", G_CALLBACK(readfromio), 0);

    btn = gtk_button_new_with_label("From Memory");
    g_signal_connect(btn, "clicked", G_CALLBACK(readfrommem), 0);
    gtk_box_pack_start(GTK_BOX(buttonbox1), btn, 0, 0, 1);
    btn = gtk_button_new_with_label("To IO");
    g_signal_connect(btn, "clicked", G_CALLBACK(writetoio), 0);
    gtk_box_pack_start(GTK_BOX(buttonbox2), btn, 0, 0, 1);
    btn = gtk_button_new_with_label("To Memory");
    gtk_box_pack_start(GTK_BOX(buttonbox2), btn, 0, 0, 1);
    g_signal_connect(btn, "clicked", G_CALLBACK(writetomem), 0);

    gtk_widget_set_hexpand(view, 1);
    gtk_widget_set_vexpand(view, 1);
    gtk_widget_show_all (window);
    pthread_mutex_init(&sim_mutex, 0);
    pthread_mutex_init(&broadcast_mutex, 0);
    g_timeout_add(10, idleupdate, 0);

    //connect ppi to simulator
    ppi_8255 ppi;
    connect_ppi(&ppi, &simulator, 10);
    gtk_main ();

    return 0;
}
