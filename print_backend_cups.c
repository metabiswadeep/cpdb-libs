#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <cups/cups.h>
#include "backend_interface.h"
#include "print_data_structures.h"

#define _CUPS_NO_DEPRECATED 1
#define BUS_NAME "org.openprinting.Backend.CUPS"
#define OBJECT_PATH "/"

GDBusConnection *dbus_connection;
PrintBackend *skeleton;
GHashTable *dialog_printers;
GHashTable *dialog_cancel;
int num_frontends; // the number of frontends that are currently connected

static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer not_used);
static void on_activate_backend(GDBusConnection *connection,
                                const gchar *sender_name,
                                const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *signal_name,
                                GVariant *parameters,
                                gpointer not_used);
static void on_stop_backend(GDBusConnection *connection,
                            const gchar *sender_name,
                            const gchar *object_path,
                            const gchar *interface_name,
                            const gchar *signal_name,
                            GVariant *parameters,
                            gpointer not_used);
static void on_refresh_backend(GDBusConnection *connection,
                               const gchar *sender_name,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *signal_name,
                               GVariant *parameters,
                               gpointer not_used);
gpointer list_printers(gpointer _dialog_name);
int send_printer_added(void *_dialog_name, unsigned flags, cups_dest_t *dest);
//gpointer find_removed_printers(gpointer not_used);
int add_printer_to_list(void *user_data, unsigned flags, cups_dest_t *dest);

static gboolean on_handle_list_basic_options(PrintBackend *interface,
                                             GDBusMethodInvocation *invocation,
                                             const gchar *printer_name,
                                             gpointer user_data);
int main()
{
    dbus_connection = NULL;
    skeleton = NULL;
    dialog_cancel = g_hash_table_new(g_str_hash, g_str_equal); /// to do : add destroy functions
    dialog_printers = g_hash_table_new(g_str_hash, g_str_equal);
    num_frontends = 0;

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_bus_own_name(G_BUS_TYPE_SESSION,
                   BUS_NAME,
                   0,                //flags
                   NULL,             //bus_acquired_handler
                   on_name_acquired, //name acquired handler
                   NULL,             //name_lost handler
                   NULL,             //user_data
                   NULL);            //user_data free function
    g_main_loop_run(loop);
}

static void
on_name_acquired(GDBusConnection *connection,
                 const gchar *name,
                 gpointer not_used)
{
    dbus_connection = connection;

    GError *error = NULL;

    skeleton = print_backend_skeleton_new();

    g_signal_connect(skeleton,                                 //instance
                     "handle-list-basic-options",              //signal name
                     G_CALLBACK(on_handle_list_basic_options), //callback
                     NULL);                                    //user_data

    /**subscribe to signals **/
    g_dbus_connection_signal_subscribe(connection,
                                       NULL,                             //Sender name
                                       "org.openprinting.PrintFrontend", //Sender interface
                                       ACTIVATE_BACKEND_SIGNAL,          //Signal name
                                       NULL,                             /**match on all object paths**/
                                       NULL,                             /**match on all arguments**/
                                       0,                                //Flags
                                       on_activate_backend,              //callback
                                       NULL,                             //user_data
                                       NULL);
    g_dbus_connection_signal_subscribe(connection,
                                       NULL,                             //Sender name
                                       "org.openprinting.PrintFrontend", //Sender interface
                                       STOP_BACKEND_SIGNAL,              //Signal name
                                       NULL,                             /**match on all object paths**/
                                       NULL,                             /**match on all arguments**/
                                       0,                                //Flags
                                       on_stop_backend,                  //callback
                                       NULL,                             //user_data
                                       NULL);
    g_dbus_connection_signal_subscribe(connection,
                                       NULL,                             //Sender name
                                       "org.openprinting.PrintFrontend", //Sender interface
                                       REFRESH_BACKEND_SIGNAL,           //Signal name
                                       NULL,                             /**match on all object paths**/
                                       NULL,                             /**match on all arguments**/
                                       0,                                //Flags
                                       on_refresh_backend,               //callback
                                       NULL,                             //user_data
                                       NULL);
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skeleton), connection, OBJECT_PATH, &error);
    g_assert_no_error(error);
}

static void on_activate_backend(GDBusConnection *connection,
                                const gchar *sender_name,
                                const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *signal_name,
                                GVariant *parameters,
                                gpointer not_used)
{

    //g_message("Enumerating printers for %s\n", sender_name);

    char *t = malloc(sizeof(gchar) * (strlen(sender_name) + 1));
    strcpy(t, sender_name);

    int *cancel = malloc(sizeof(int));
    *cancel = 0;
    g_hash_table_insert(dialog_cancel, t, cancel);

    GHashTable *g = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(dialog_printers, t, g);
    num_frontends++;
    g_thread_new(NULL, list_printers, t);
    // if (num_frontends == 1) // you need to wake this thread up!
    // {
    //     g_thread_new(NULL, find_removed_printers, NULL);
    // }
    // g_message("Leaving this function: the threads will take care.\n");
}

gpointer list_printers(gpointer _dialog_name)
{
    gchar *dialog_name = (gchar *)_dialog_name;
    g_message("New thread for dialog at %s\n", dialog_name);
    int *cancel = (int *)(g_hash_table_lookup(dialog_cancel, dialog_name));
    *cancel = 0;

    cupsEnumDests(CUPS_DEST_FLAGS_NONE,
                  -1, //NO timeout
                  cancel,
                  0, //TYPE
                  0, //MASK
                  send_printer_added,
                  _dialog_name);

    //g_hash_table_remove(dialog_cancel, dialog_name);
    //g_hash_table_destroy(g_hash_table_lookup(dialog_printers, dialog_name));
    //g_hash_table_remove(dialog_printers, dialog_name); //huge memory leak
    g_message("Exiting thread for dialog at %s\n", dialog_name);
}

int send_printer_added(void *_dialog_name, unsigned flags, cups_dest_t *dest)
{

    char *dialog_name = (char *)_dialog_name;

    GHashTable *g = g_hash_table_lookup(dialog_printers, dialog_name);
    g_assert_nonnull(g);

    if (g_hash_table_contains(g, dest->name))
    {
        g_message("%s already sent.\n", dest->name);
        return 1;
    }

    char *t = malloc(sizeof(gchar) * (strlen(dest->name) + 1));
    strcpy(t, dest->name);
    g_hash_table_add(g, t);
    GVariant *gv = g_variant_new("(sssss)",
                                 t,
                                 cupsGetOption("printer-info", dest->num_options, dest->options),
                                 cupsGetOption("printer-location", dest->num_options, dest->options),
                                 cupsGetOption("printer-make-and-model", dest->num_options, dest->options),
                                 cupsGetOption("printer-is-accepting-jobs", dest->num_options, dest->options));

    GError *error = NULL;
    g_dbus_connection_emit_signal(dbus_connection,
                                  dialog_name,
                                  OBJECT_PATH,
                                  "org.openprinting.PrintBackend",
                                  PRINTER_ADDED_SIGNAL,
                                  gv,
                                  &error);
    g_assert_no_error(error);
    g_message("     Sent notification for printer %s\n", dest->name);

    ///fix memory leaks
    return 1; //continue enumeration
}
static void on_refresh_backend(GDBusConnection *connection,
                               const gchar *sender_name,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *signal_name,
                               GVariant *parameters,
                               gpointer not_used)
{
    g_message("Refresh backend signal from %s\n", sender_name);
    int *cancel = (int *)(g_hash_table_lookup(dialog_cancel, sender_name));
    *cancel = 1;
    //fix memory leak here
    GHashTable *g = g_hash_table_new(g_str_hash, g_str_equal);
    GHashTableIter iter;
    gpointer key;
    GHashTable *prev = g_hash_table_lookup(dialog_printers, sender_name);
    cupsEnumDests(CUPS_DEST_FLAGS_NONE,
                  1000,                //timeout
                  NULL,                //cancel
                  0,                   //TYPE
                  0,                   //MASK
                  add_printer_to_list, //function
                  g);                  //user_data
    g_hash_table_iter_init(&iter, prev);
    while (g_hash_table_iter_next(&iter, &key, NULL))
    {
        //g_message("                                             .. %s ..\n", (gchar *)key);
        if (!g_hash_table_contains(g, (gchar *)key))
        {
            g_message("Printer %s removed\n", (char *)key);
            //print_backend_emit_printer_removed(skeleton, (char *)key);
            g_dbus_connection_emit_signal(dbus_connection,
                                          sender_name,
                                          OBJECT_PATH,
                                          "org.openprinting.PrintBackend",
                                          PRINTER_REMOVED_SIGNAL,
                                          g_variant_new("(s)", (char *)key),
                                          NULL);
        }
    }

    char *t = malloc(sizeof(gchar) * (strlen(sender_name) + 1));
    strcpy(t, sender_name);
    g_hash_table_replace(dialog_printers, t, g);
    g_thread_new(NULL, list_printers, t);
    //g_message("Hi\n");
    ///call cupsEnumDests once .. check with current hash table.
    //send printer added/removed signals accordingly
}
int add_printer_to_list(void *user_data, unsigned flags, cups_dest_t *dest)
{
    GHashTable *h = (GHashTable *)user_data;
    char *printername = strdup(dest->name);
    g_hash_table_add(h, printername);
}
static void on_stop_backend(GDBusConnection *connection,
                            const gchar *sender_name,
                            const gchar *object_path,
                            const gchar *interface_name,
                            const gchar *signal_name,
                            GVariant *parameters,
                            gpointer not_used)
{
    g_message("Stop backend signal from %s\n", sender_name);
    int *cancel = (int *)(g_hash_table_lookup(dialog_cancel, sender_name));
    *cancel = 1;
    num_frontends--;
}
gboolean on_handle_list_basic_options(PrintBackend *interface,
                                      GDBusMethodInvocation *invocation,
                                      const gchar *printer_name,
                                      gpointer user_data)
{
    g_message("Listing basic options");
    cups_dest_t *dest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, printer_name, NULL);
    g_assert_nonnull(dest);
    print_backend_complete_list_basic_options(interface, invocation,
                                              cupsGetOption("printer-info", dest->num_options, dest->options),
                                              cupsGetOption("printer-location", dest->num_options, dest->options),
                                              cupsGetOption("printer-make-and-model", dest->num_options, dest->options),
                                              cupsGetOption("printer-is-accepting-jobs", dest->num_options, dest->options));
    return TRUE;
}
// gpointer find_removed_printers(gpointer not_used)
// {
//     g_message("Starting find_removed_printers thread ..\n");
//     GHashTable *set[2];
//     set[0] = g_hash_table_new(g_str_hash, g_str_equal);
//     set[1] = g_hash_table_new(g_str_hash, g_str_equal);
//     int curr = 0, prev = 1;
//     GHashTableIter iter;
//     gpointer key;
//     while (num_frontends > 0)
//     {
//         //fix memory leaks
//         // g_message("curr,prev = %d,%d",curr,prev);
//         g_hash_table_remove_all(set[curr]);
//         cupsEnumDests(CUPS_DEST_FLAGS_NONE,
//                       4500,                //timeout in ms
//                       NULL,                //cancel variable
//                       0,                   //TYPE
//                       0,                   //MASK
//                       add_printer_to_list, // Callback function
//                       set[curr]);          //user_data

//         g_hash_table_iter_init(&iter, set[prev]);
//         while (g_hash_table_iter_next(&iter, &key, NULL))
//         {
//             //g_message("                                             .. %s ..\n",(gchar*)key);
//             if (!g_hash_table_contains(set[curr], (gchar *)key))
//             {
//                 g_message("Printer %s removed\n", (char *)key);
//                 print_backend_emit_printer_removed(skeleton, (char *)key);
//             }
//         }
//         curr = 1 - curr; //switching it over 0<-->1; so that the curr becomes prev and prev becomes curr
//         prev = 1 - prev;
//     }
//     g_hash_table_destroy(set[0]);
//     g_hash_table_destroy(set[1]);

//     g_message("find_removed_printers thread exited\n");
// }
// int add_printer_to_list(void *user_data, unsigned flags, cups_dest_t *dest)
// {
//     GHashTable *h = (GHashTable *)user_data;
//     char *printername = strdup(dest->name);
//     g_hash_table_add(h, printername);
// }