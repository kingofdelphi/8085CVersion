#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
gchar * program;
char programpath[255];
GtkWidget *window;
GtkWidget *grid;
GtkWidget *openBtn, * quitBtn, *toolbar;
GtkWidget *view;
GtkTextBuffer *buffer;
//flags
GtkWidget * signlbl, * zerolbl, * auxlbl, * paritylbl, * carrylbl;
//registers
GtkWidget * rega, * regb, * regc, * regd, * rege, * regh, * regl, * regsp, * regpc;
GtkTextTag * labeltag;
GtkTextTag * instructiontag;
GtkTextTag * defaulttag;
GtkTextTag * registertag;
GtkTextTag * commenttag;

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
int NSTRCMP(const char * a, const char * b, int bn) {
    char tmp[255] = {0};
    strncat(tmp, b, bn);
    return strcmp(a, tmp);
}
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
    return s[n - 1] == ':';
}
int iscomment(const char * s, int n) {
    return *s == '$';
}
int isdelims(char ch) {
    return ch == ',' || ch == '$' || ch == ':';
}
void gettoken(const char * s, int * i, int * tok_s, int * tok_e) {
    *tok_s = *tok_e = 0;
    int n = strlen(s);
    while (*i < n && isspace(s[*i])) {
        ++*i;
    }
    if (*i < n && s[*i] == '$') {
        *tok_s = *i;
        *tok_e = n;
        *i = n; //do not parse rest of comment line
        return ;
    }
    if (*i < n && isdelims(s[*i])) {
        *tok_s = *i;
        *tok_e = *i + 1;
        ++*i;
        return ;
    }
    *tok_s = *tok_e = *i;
    while (*i < n && !isspace(s[*i]) && !isdelims(s[*i])) {
        ++*tok_e;
        ++*i;
    }
    while (*i < n && isspace(s[*i])) ++*i;
    if (*i < n && s[*i] == ':') {
        *tok_e = *i + 1;
        ++*i;
    }
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
    for (int i = 0; i < lines; ++i) {
        GtkTextIter is, ie;
        gtk_text_buffer_get_iter_at_line(buffer, &is, i);
        gtk_text_buffer_get_iter_at_line(buffer, &ie, i);
        if (!gtk_text_iter_ends_line(&ie)) gtk_text_iter_forward_to_line_end(&ie);
        gchar * txt = gtk_text_buffer_get_text(buffer, &is, &ie, 0);
        //g_print("line %d  |%s|\n", i, (const char *)txt);
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
                gtk_text_buffer_apply_tag (buffer, commenttag, &tstart, &tend);
            }
        }
        g_free(txt);
    }
}

void changed() {
    recolorBuffer();
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
    GtkToolItem* tbtn = gtk_tool_button_new(gtk_image_new_from_file("new.ico"), "New");
    gtk_toolbar_insert(GTK_TOOLBAR(navbar), tbtn, 0);
    tbtn = gtk_tool_button_new(gtk_image_new_from_file("open.ico"), "Open");
    g_signal_connect(tbtn, "clicked", G_CALLBACK(openfiledialog), 0);

    gtk_toolbar_insert(GTK_TOOLBAR(navbar), tbtn, 1);
    tbtn = gtk_tool_button_new(gtk_image_new_from_file("run.ico"), "Run");
    gtk_toolbar_insert(GTK_TOOLBAR(navbar), tbtn, 2);
    tbtn = gtk_tool_button_new(gtk_image_new_from_file("run.ico"), "Step");
    gtk_toolbar_insert(GTK_TOOLBAR(navbar), tbtn, 3);

    //start placing objects onto the window
    GtkWidget * mainbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget * menubox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * editor_scroll = gtk_scrolled_window_new(0, 0);
    GtkWidget * infobox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget * corebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * flagbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * reghorz1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * reghorz2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * reghorz3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * reghorz4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * spbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget * pcbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_container_add(GTK_CONTAINER(window), mainbox);
    gtk_container_add(GTK_CONTAINER(mainbox), menubox);
    gtk_container_add(GTK_CONTAINER(mainbox), corebox);
    gtk_container_add(GTK_CONTAINER(corebox), editor_scroll);
    gtk_container_add(GTK_CONTAINER(corebox), infobox);

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


    gtk_widget_set_hexpand(view, 1);
    gtk_widget_set_vexpand(view, 1);
    gtk_widget_show_all (window);

    gtk_main ();

    return 0;
}
