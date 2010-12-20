#include "geanyplugin.h"
#include "plugincommon.h"
#include "main_notebook.h"

static gboolean holds_main_notebook(GtkWidget *widget);
static GtkWidget *create_main_notebook(void);

/**
 * Checks to see if the main_notebook exists in Geany's UI.
 * 
 * @return	TRUE if the main_notebook exists, FALSE if not.
 */
gboolean main_notebook_exists(void)
{
	return holds_main_notebook(geany->main_widgets->window);
}

/**
 * Checks to see if the notebook needs to be destroyed, as in whether it
 * even exists or if it only contains the code tab (ie. last tab).  This
 * function gets called from main_notebook_destroy() to ensure that you
 * can't pull the rug out from under another plugin using the notebook.
 * 
 * @return	TRUE if there are no other plugins using the main notebook
 * 				and it's safe to destroy or FALSE if it should be left
 * 				alone.
 */
gboolean main_notebook_needs_destroying(void)
{
	if (main_notebook_exists())
	{
		GtkWidget *nb = main_notebook_get();
		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(nb)) > 1)
			return FALSE; /* don't destroy, other plugins using nb */
		return TRUE; /* only code page left, ok to destroy */
	}
	return FALSE; /* nb not exists, so don't tell caller to destroy it */	
}

/**
 * Puts Geany's UI back to the way it was before the main_notebook was
 * added in.  This function won't actually do anything if another plugin
 * is still using the main_notebook.
 */
void main_notebook_destroy(void)
{	
	/* skip destroying the main_notebook if other plugins are using it */
	if (!main_notebook_needs_destroying())
		return;
		
	GtkWidget *main_notebook, *doc_nb_parent, *vbox;

	main_notebook = ui_lookup_widget(geany->main_widgets->window, 
									"main_notebook");

	/* this is the original place where Geany's doc notebook was and where it
	 * needs to be put back to */
	doc_nb_parent = gtk_widget_get_parent(main_notebook);
	
	/* a temporary place to put Geany's doc notebook so it stays visible */
	vbox = ui_lookup_widget(geany->main_widgets->window, "vbox1");
	
	/* move Geany's doc notebook to the temp location */
	gtk_widget_reparent(geany->main_widgets->notebook, vbox);
	
	/* get rid of the main notebook and webview stuff */
	gtk_widget_destroy(main_notebook);
	
	/* put Geany's doc notebook back to its original location */
	gtk_widget_reparent(geany->main_widgets->notebook, doc_nb_parent);
}

/**
 * Checks to see if the main_notebook already exists and if it does, it
 * returns a pointer to it, if not, it creates the main_notebook.  This 
 * function should be called by plugins wishing to use the main_notebook
 * to ensure that there is no conflict with other plugins.
 * 
 * @return The main_notebook that already existed or was created.  If
 * 			 there was a problem creating/getting the main_notebook,
 * 			 NULL is returned.
 */
GtkWidget *main_notebook_get(void)
{
	if (main_notebook_exists())
		return ui_lookup_widget(geany->main_widgets->window, "main_notebook");
	else
		return create_main_notebook();
}

/* we need this since ui_lookup_widget() doesn't seem to realize when
 * the main_notebook widget is destroyed and so keeps returning something
 * even when main_notebook is gone. */
static gboolean holds_main_notebook(GtkWidget *widget)
{
    gboolean found = FALSE;
    const gchar *widget_name = gtk_widget_get_name(widget);
    
    if (widget_name != NULL && g_strcmp0(widget_name, "main_notebook") == 0)
        found = TRUE;
    if (GTK_IS_CONTAINER(widget))
    {
        GList *children, *iter;
        
        children = gtk_container_get_children(GTK_CONTAINER(widget));
        
        for (iter=children; !found && iter; iter=g_list_next(iter))
            found = holds_main_notebook(iter->data);
            
        g_list_free(children);
    }
    
    return found;
}

/* 
 * Creates the main_notebook if it doesn't already exist and returns a
 * pointer to it.  If it does already exist, the function returns NULL.
 */
static GtkWidget *create_main_notebook(void)
{
	GtkWidget *main_notebook, *doc_nb_box, *doc_nb_parent, *code_label, *vbox;
	
	/* bail out if the main notebook exists already */
	if (main_notebook_exists())
		return NULL;
	
	code_label = gtk_label_new(_("Code"));
	
	/* create the new main_notebook */
	main_notebook = gtk_notebook_new();
	gtk_widget_set_name(main_notebook, "main_notebook");
	
	/* this is going to hold Geany's document notebook */
	doc_nb_box = gtk_vbox_new(FALSE, 0);
	
	/* put the box where Geany's documents notebook will live into the main
	 * notebook */
	gtk_notebook_append_page(GTK_NOTEBOOK(main_notebook), doc_nb_box, code_label);
	
	/* find a place to put Geany's documents notebook temporarily */
	vbox = ui_lookup_widget(geany->main_widgets->window, "vbox1");
	
	/* this is where our new main notebook is going to go */
	doc_nb_parent = gtk_widget_get_parent(geany->main_widgets->notebook);
	
	/* move the geany doc notebook widget into the main vbox temporarily */
	/* see splitwindow.c */
	gtk_widget_reparent(geany->main_widgets->notebook, vbox);
	
	/* add the new main notebook to where Geany's documents notebook was */
	gtk_container_add(GTK_CONTAINER(doc_nb_parent), main_notebook);

	/* make sure it's all visible */
	gtk_widget_show_all(main_notebook);
   
	/* put Geany's doc notebook into it's new home */
	gtk_widget_reparent(geany->main_widgets->notebook, doc_nb_box);
	
	/* set the code tab as current */
	gtk_notebook_set_current_page(GTK_NOTEBOOK(main_notebook), 0);

	/* move the tabs to the bottom */
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(main_notebook), GTK_POS_BOTTOM);

	/* from ui_utils: ui_hookup_widget() */
	g_object_set_data_full(
		G_OBJECT(geany->main_widgets->window), 
		"main_notebook",
		g_object_ref(main_notebook),  
		(GDestroyNotify)g_object_unref);

	return main_notebook;
}
