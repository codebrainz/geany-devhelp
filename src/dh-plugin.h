#ifndef DH_PLUGIN_H
#define DH_PLUGIN_H

#ifndef DHPLUG_DATA_DIR
#define DHPLUG_DATA_DIR "/usr/local/share/geany-devhelp"
#endif

#define DHPLUG_WEBVIEW_HOME_FILE DHPLUG_DATA_DIR"/home.html"

/* main plugin struct */
typedef struct
{
	GtkWidget *book_tree;			/// "Contents" in the sidebar
	GtkWidget *search;				/// "Search" in the sidebar
	GtkWidget *sb_notebook;			/// Notebook that holds contents/search
	gint sb_notebook_tab;			/// Index of tab where devhelp sidebar is
	GtkWidget *webview;				/// Webkit that shows documentation
	gint webview_tab;				/// Index of tab that contains the webview
	GtkWidget *main_notebook;		/// Notebook that holds Geany doc notebook and
									/// and webkit view
	GtkWidget *doc_notebook;		/// Geany's document notebook  
	GtkWidget *editor_menu_item;	/// Item in the editor's context menu 
	GtkWidget *editor_menu_sep;		/// Separator item above menu item
	gboolean *webview_active;		/// Tracks whether webview stuff is shown
	
	gboolean last_main_tab_id;		/// These track the last id of the tabs
	gboolean last_sb_tab_id;		///   before toggling
	gboolean tabs_toggled;			/// Tracks state of whether to toggle to
									/// Devhelp or back to code
	gboolean created_main_nb;		/// Track whether we created the main notebook

} DevhelpPlugin;

gchar *devhelp_clean_word(gchar *str);
gchar *devhelp_get_current_tag(void);
void devhelp_activate_tabs(DevhelpPlugin *dhplug, gboolean contents);
DevhelpPlugin *devhelp_plugin_new(void);
void devhelp_plugin_destroy(DevhelpPlugin *dhplug);

#endif
