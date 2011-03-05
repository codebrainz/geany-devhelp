#ifndef MAIN_NOTEBOOK_H
#define MAIN_NOTEBOOK_H

/* 
 * When you need the main_notebook, you call main_notebook_get() which 
 * will get an existing main_notebook or create one if it doesn't exist.  
 * When you are done with the main_notebook, call main_notebook_destroy() 
 * and it will be destroyed and the UI will be put back to normal only 
 * if no other plugins are still using it.
 * 
 * See main-notebook.c for documentation for these functions
 */

gboolean main_notebook_exists(void);
GtkWidget *main_notebook_get(void);
gboolean main_notebook_needs_destroying(void);
void main_notebook_destroy(void);

#endif
