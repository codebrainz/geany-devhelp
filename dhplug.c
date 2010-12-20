/*
 * dhplug.c
 * 
 * Copyright 2010 Matthew Brush <mbrush@leftclick.ca>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "geanyplugin.h"

#include <gdk/gdkkeysyms.h> /* for keybindings */

#include <devhelp/dh-base.h>
#include <devhelp/dh-book-tree.h>
#include <devhelp/dh-search.h>
#include <devhelp/dh-link.h>

#ifdef HAVE_BOOK_MANAGER /* for newer api */
#include <devhelp/dh-book-manager.h>
#endif

#include <webkit/webkitwebview.h>

/* todo: make a better homepage as a separate file with images installed
 *	   into the proper directory. */
#define WEBVIEW_HOMEPAGE "<html><body><h1 align=\"center\">Geany Devhelp " \
			"Plugin</h1><p align=\"center\">Use the Devhelp sidebar tab to " \
			"navigate the documentation.</p><p align=\"center\"><span " \
			"style=\"font-weight: bold;\">Tip:</span> Select a tag/symbol "\
			"in your code, right click on the editor and choose the 'Search " \
			"Documentation for Tag' option to search for that tag in Devhelp." \
			"</p></body></html>"

GeanyPlugin	 	*geany_plugin;
GeanyData	   		*geany_data;
GeanyFunctions  	*geany_functions;

PLUGIN_VERSION_CHECK(200)

PLUGIN_SET_INFO(
	_("Devhelp Plugin"), 
	_("Adds built-in Devhelp support."),
	_("1.0"), _("Matthew Brush <mbrush@leftclick.ca>"))

/* Devhelp base object */
static DhBase *dhbase = NULL;  

/* main plugin struct */
typedef struct
{
	GtkWidget *book_tree;			/// "Contents" in the sidebar
	GtkWidget *search;				/// "Search" in the sidebar
	GtkWidget *sb_notebook;		/// Notebook that holds contents/search
	gint sb_notebook_tab;			/// Index of tab where devhelp sidebar is
	GtkWidget *webview;			/// Webkit that shows documentation
	gint webview_tab;				/// Index of tab that contains the webview
	GtkWidget *main_notebook;		/// Notebook that holds Geany doc notebook and
									/// and webkit view
	GtkWidget *doc_notebook;		/// Geany's document notebook  
	GtkWidget *editor_menu_item;	/// Item in the editor's context menu 
	GtkWidget *editor_menu_sep;	/// Separator item above menu item
	gboolean *webview_active;		/// Tracks whether webview stuff is shown
	
	gboolean last_main_tab_id;		/// These track the last id of the tabs
	gboolean last_sb_tab_id;		///   before toggling
	gboolean tabs_toggled;		/// Tracks state of whether to toggle to
									/// Devhelp or back to code

} DevhelpPlugin;

/* 
 * Replaces non GEANY_WORDCHARS in str with spaces and then trims whitespace.
 * This function does not allocate a new string, it modifies str in place
 * and returns a pointer to str. 
 * TODO: make this only remove stuff from the start or end of string.
 */
gchar *clean_word(gchar *str)
{
	return g_strstrip(g_strcanon(str, GEANY_WORDCHARS, ' '));
}

/* Gets either the current selection or the word at the current selection. */
gchar *get_current_tag(void)
{
	gint pos;
	gchar *tag = NULL;
	GeanyDocument *doc = document_get_current();
	
	if (sci_has_selection(doc->editor->sci))
		return clean_word(sci_get_selection_contents(doc->editor->sci));
	
	pos = sci_get_current_position(doc->editor->sci);
	
	tag = editor_get_word_at_pos(doc->editor, pos, GEANY_WORDCHARS);
	
	if (tag == NULL) return NULL;
	
	if (tag[0] == '\0') {
		g_free(tag);
		return NULL;
	}
	
	return clean_word(tag);
}

/**
 * Toggles devhelp related tabs to be current tabs or back to the
 * previously selected tabs.
 * 
 * @param	dhplug		The current DevhelpPlugin struct.
 * @param	contents	If TRUE then select the devhelp Contents tab
 * 						otherwise select the devhelp Search tab.
 */ 
void devhelp_activate_tabs(DevhelpPlugin *dhplug, gboolean contents)
{
	if (!dhplug->tabs_toggled)
	{
		/* toggle state tracking */
		dhplug->last_main_tab_id = gtk_notebook_get_current_page(
										GTK_NOTEBOOK(dhplug->main_notebook));
		dhplug->last_sb_tab_id = gtk_notebook_get_current_page(GTK_NOTEBOOK(
										geany->main_widgets->sidebar_notebook));
		dhplug->tabs_toggled = TRUE;
		
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), 
			dhplug->sb_notebook_tab);
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(dhplug->main_notebook), dhplug->webview_tab);
		if (contents)
			gtk_notebook_set_current_page(GTK_NOTEBOOK(dhplug->sb_notebook), 0);
		else
			gtk_notebook_set_current_page(GTK_NOTEBOOK(dhplug->sb_notebook), 1);
	}
	else
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(dhplug->main_notebook), 
			dhplug->last_main_tab_id);
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
			dhplug->last_sb_tab_id);
		dhplug->tabs_toggled = FALSE;
	}
}

/* Called when the editor menu item is selected */
void on_search_help_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	DevhelpPlugin *dhplug = user_data;
	gchar *current_tag = get_current_tag();
	
	if (current_tag == NULL)
		return;
	
	dh_search_set_search_string (DH_SEARCH(dhplug->search), current_tag, NULL);
	
	/* activate devhelp tabs with search tab active */
	devhelp_activate_tabs(dhplug, FALSE);
	
	g_free(current_tag);
}

/* 
 * Called when the editor context menu is shown so that the devhelp
 * search item can be disabled if there isn't a selected tag.
 */
void on_editor_menu_popup(GtkWidget *widget, gpointer user_data)
{
	gchar *curword = NULL;
	DevhelpPlugin *dhplug = user_data;
	
	curword = get_current_tag();
	if (curword == NULL)
		gtk_widget_set_sensitive(dhplug->editor_menu_item, FALSE);
	else
		gtk_widget_set_sensitive(dhplug->editor_menu_item, TRUE);
	
	g_free(curword);	
}

/**
 * Called when a link in either the contents or search areas on the sidebar 
 * have a link clicked on, meaning to load that file into the webview.
 * @param ignored		Not used
 * @param link	  		The devhelp link object describing what was clicked.
 * @param user_data 	The current DevhelpPlugin struct.
 */
void on_link_clicked(GObject *ignored, DhLink *link, gpointer user_data)
{
	gchar *uri = dh_link_get_uri(link);
	DevhelpPlugin *plug = user_data;
	webkit_web_view_open(WEBKIT_WEB_VIEW(plug->webview), uri);
	g_free(uri);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(plug->main_notebook), plug->webview_tab);
}

/**
 * Creates a new DevhelpPlugin.  The returned structure is allocated dyamically
 * and must be freed with the devhelp_plugin_destroy() function.  This function
 * gets called from Geany's plugin_init() function.
 * @return A newly allocated DevhelpPlugin struct or null on error.
 */
DevhelpPlugin *devhelp_plugin_new(void)
{
	GtkWidget *doc_notebook_box, *book_tree_sw, *webview_sw;
	GtkWidget *contents_label, *search_label, *dh_sidebar_label;
	GtkWidget *code_label, *doc_label;
	GtkWidget *doc_notebook_parent, *vbox;

#ifdef HAVE_BOOK_MANAGER /* for newer api */
	DhBookManager *book_manager;
#else
	GNode *books;
	GList *keywords;
#endif

	DevhelpPlugin *dhplug = g_malloc0(sizeof(DevhelpPlugin));
	
	if (dhplug == NULL) {
		g_printerr(_("Cannot create a new Devhelp plugin, out of memory.\n"));
		return NULL;
	}
	
	if (dhbase == NULL)
		dhbase = dh_base_new();	
		
#ifdef HAVE_BOOK_MANAGER /* for newer api */
	book_manager = dh_base_get_book_manager(dhbase);
	dhplug->book_tree = dh_book_tree_new(book_manager);
	dhplug->search = dh_search_new(book_manager);
#else	
	books = dh_base_get_book_tree(dhbase);
	keywords = dh_base_get_keywords(dhbase);
	dhplug->book_tree = dh_book_tree_new(books);
	dhplug->search = dh_search_new(keywords);
#endif
	
	/* create main widgets */
	dhplug->sb_notebook = gtk_notebook_new();
	dhplug->main_notebook = gtk_notebook_new();
	dhplug->doc_notebook = geany->main_widgets->notebook;
	
	/* editor menu items */
	dhplug->editor_menu_sep = gtk_separator_menu_item_new();
	dhplug->editor_menu_item = gtk_menu_item_new_with_label(
									_("Search Documentation for Tag"));
	   
	/* tab labels */
	contents_label = gtk_label_new(_("Contents"));
	search_label = gtk_label_new(_("Search"));
	dh_sidebar_label = gtk_label_new(_("Devhelp"));
	code_label = gtk_label_new(_("Code"));
	doc_label = gtk_label_new(_("Documentation"));	
	
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), 
		GTK_POS_BOTTOM);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(dhplug->main_notebook), GTK_POS_BOTTOM);

	/* sidebar contents/book tree */
	book_tree_sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(book_tree_sw),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(book_tree_sw), 6);
	gtk_container_add(GTK_CONTAINER(book_tree_sw), dhplug->book_tree); 
	gtk_widget_show(dhplug->book_tree);
	
	/* sidebar search */
	gtk_widget_show(dhplug->search);
	
	/* webview to display documentation */
	dhplug->webview = webkit_web_view_new();
	webview_sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(webview_sw),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(webview_sw), 6);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(webview_sw), 
		GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(webview_sw), dhplug->webview);
	gtk_widget_show_all(webview_sw);
	
	/* setup the sidebar notebook */
	gtk_notebook_append_page(GTK_NOTEBOOK(dhplug->sb_notebook),
		book_tree_sw, contents_label);
	gtk_notebook_append_page(GTK_NOTEBOOK(dhplug->sb_notebook),
		dhplug->search, search_label);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(dhplug->sb_notebook), 0);
		
	gtk_widget_show_all(dhplug->sb_notebook);
	gtk_notebook_append_page(
		GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
		dhplug->sb_notebook, dh_sidebar_label);
	dhplug->sb_notebook_tab = gtk_notebook_page_num(
		GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
		dhplug->sb_notebook);
	
	/* this is going to hold Geany's document notebook */
	doc_notebook_box = gtk_vbox_new(FALSE, 0);
	
	/* put the box where Geany's documents notebook will live into the main
	 * notebook */
	gtk_notebook_append_page(GTK_NOTEBOOK(dhplug->main_notebook),
		doc_notebook_box, code_label);

	/* put the webview stuff into the main notebook */
	gtk_notebook_append_page(GTK_NOTEBOOK(dhplug->main_notebook),
		webview_sw, doc_label);
	dhplug->webview_tab = gtk_notebook_page_num(
							GTK_NOTEBOOK(dhplug->main_notebook), webview_sw);
	
	/* find a place to put Geany's documents notebook temporarily */
	vbox = ui_lookup_widget(geany->main_widgets->window, "vbox1");
	
	/* this is where our new main notebook is going to go */
	doc_notebook_parent = gtk_widget_get_parent(dhplug->doc_notebook);
	
	/* move the geany doc notebook widget into the main vbox temporarily */
	/* see splitwindow.c */
	gtk_widget_reparent(dhplug->doc_notebook, vbox);
	
	/* add the new main notebook to where Geany's documents notebook was */
	gtk_container_add(GTK_CONTAINER(doc_notebook_parent), dhplug->main_notebook);

	/* make sure it's all visible */
	gtk_widget_show_all(dhplug->main_notebook);
   
	/* put Geany's doc notebook into it's new home */
	gtk_widget_reparent(dhplug->doc_notebook, doc_notebook_box);
	
	/* set the code tab as current */
	gtk_notebook_set_current_page(GTK_NOTEBOOK(dhplug->main_notebook), 0);
	
	/* add menu item to editor popup menu */
	/* todo: make this an image menu item with devhelp icon */
	gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->editor_menu),
		dhplug->editor_menu_sep);
	gtk_menu_shell_append(GTK_MENU_SHELL(geany->main_widgets->editor_menu),
		dhplug->editor_menu_item);
	gtk_widget_show(dhplug->editor_menu_sep);
	gtk_widget_show(dhplug->editor_menu_item);

	/* connect signals */
	g_signal_connect(
			geany->main_widgets->editor_menu, 
			"show",
			G_CALLBACK(on_editor_menu_popup), 
			dhplug);
										
	g_signal_connect(
			dhplug->editor_menu_item, 
			"activate",
			G_CALLBACK(on_search_help_activate), 
			dhplug);
										
	g_signal_connect(
			dhplug->book_tree, 
			"link-selected", 
			G_CALLBACK(on_link_clicked), 
			dhplug);
										
	g_signal_connect(
			dhplug->search, 
			"link-selected",
			G_CALLBACK(on_link_clicked), 
			dhplug);	

	/* toggle state tracking */
	dhplug->last_main_tab_id = gtk_notebook_get_current_page(
									GTK_NOTEBOOK(dhplug->main_notebook));
	dhplug->last_sb_tab_id = gtk_notebook_get_current_page(GTK_NOTEBOOK(
									geany->main_widgets->sidebar_notebook));
	dhplug->tabs_toggled = FALSE;
	
	/* load the default homepage for the webview */
	webkit_web_view_load_string(WEBKIT_WEB_VIEW(dhplug->webview),
		WEBVIEW_HOMEPAGE,"text/html", NULL, NULL);
	
	return dhplug;
}

/**
 * Destroys the associated widgets and frees memory for a DevhelpPlugin.  This
 * should be called from Geany's plugin_cleanup() function.
 * @param dhplug	The DevhelpPlugin to destroy/free.
 */
void devhelp_plugin_destroy(DevhelpPlugin *dhplug)
{	
	GtkWidget *doc_notebook_parent, *vbox;

	/* get rid of the devhelp tab in the sidebar */
	gtk_widget_destroy(dhplug->sb_notebook);

	/* this is the original place where Geany's doc notebook was and where it
	 * needs to be put back to */
	doc_notebook_parent = gtk_widget_get_parent(dhplug->main_notebook);
	
	/* a temporary place to put Geany's doc notebook so it stays visible */
	vbox = ui_lookup_widget(geany->main_widgets->window, "vbox1");
	
	/* move Geany's doc notebook to the temp location */
	gtk_widget_reparent(dhplug->doc_notebook, vbox);
	
	/* get rid of the main notebook and webview stuff */
	gtk_widget_destroy(dhplug->main_notebook);
	
	/* put Geany's doc notebook back to its original location */
	gtk_widget_reparent(dhplug->doc_notebook, doc_notebook_parent);
   
	/* remove the editor menu items */
	gtk_widget_destroy(dhplug->editor_menu_sep);
	gtk_widget_destroy(dhplug->editor_menu_item);
   
	/* don't unref it because it complains about only being init'd once */
	/*
	if (dhbase) {
		g_object_unref(G_OBJECT(dhbase));
		dhbase = NULL;
	}
	*/
	
	// Move geany's sidebar tabs back to the top
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), 
		GTK_POS_TOP);
	
	g_free(dhplug);
}

//------------------------------------------------------------------------------

/* keybindings */
enum
{
	KB_DEVHELP_TOGGLE_CONTENTS,
	KB_DEVHELP_TOGGLE_SEARCH,
	KB_DEVHELP_SEARCH_SYMBOL,
	KB_COUNT
};

static DevhelpPlugin *dev_help_plugin = NULL;

/* Called when a keybinding is activated */
static void kb_activate(guint key_id)
{
	switch (key_id)
	{
		case KB_DEVHELP_TOGGLE_CONTENTS:
			devhelp_activate_tabs(dev_help_plugin, TRUE);
			break;
		case KB_DEVHELP_TOGGLE_SEARCH:
			devhelp_activate_tabs(dev_help_plugin, FALSE);
			break;
		case KB_DEVHELP_SEARCH_SYMBOL:
		{
			gchar *current_tag = get_current_tag();
			if (current_tag == NULL) return;
			dh_search_set_search_string(
				DH_SEARCH(dev_help_plugin->search), current_tag, NULL);
			devhelp_activate_tabs(dev_help_plugin, FALSE);
			g_free(current_tag);
			break;
		}
	}
}

void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *key_group;
	
	/* stop crashing when the plugin is re-loaded */
	plugin_module_make_resident(geany_plugin);

	dev_help_plugin = devhelp_plugin_new();

	/* setup keybindings */
	key_group = plugin_set_key_group(geany_plugin, "devhelp", KB_COUNT, NULL);
	
	keybindings_set_item(key_group, KB_DEVHELP_TOGGLE_CONTENTS, kb_activate,
		0, 0, "devhelp_toggle_contents", _("Toggle Devhelp (Contents Tab)"), NULL);
	keybindings_set_item(key_group, KB_DEVHELP_TOGGLE_SEARCH, kb_activate,
		0, 0, "devhelp_toggle_search", _("Toggle Devhelp (Search Tab)"), NULL);
	keybindings_set_item(key_group, KB_DEVHELP_SEARCH_SYMBOL, kb_activate,
		0, 0, "devhelp_search_symbol", _("Search for Current Symbol/Tag"), NULL);
	
}

void plugin_cleanup(void)
{
	
	if (dev_help_plugin != NULL)
		devhelp_plugin_destroy(dev_help_plugin);

}
