#include <geanyplugin.h>
#include <gdk/gdkkeysyms.h> /* for keybindings */
#include <devhelp/dh-search.h>
#include <sys/stat.h>
#include "plugin.h"
#include "dh-plugin.h"

PLUGIN_VERSION_CHECK(200)

PLUGIN_SET_INFO(
	_("Devhelp Plugin"), 
	_("Adds built-in Devhelp support."),
	"1.0", "Matthew Brush <mbrush@leftclick.ca>")
	
DevhelpPlugin *dev_help_plugin = NULL;

static StashGroup *sg_user = NULL;
static gchar *default_config = NULL;
static gchar *user_config = NULL;
static gboolean move_sidebar_tabs_bottom;

/* keybindings */
enum
{
	KB_DEVHELP_TOGGLE_CONTENTS,
	KB_DEVHELP_TOGGLE_SEARCH,
	KB_DEVHELP_SEARCH_SYMBOL,
	KB_COUNT
};

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
			gchar *current_tag = devhelp_get_current_tag();
			if (current_tag == NULL) return;
			dh_search_set_search_string(
				DH_SEARCH(dev_help_plugin->search), current_tag, NULL);
			devhelp_activate_tabs(dev_help_plugin, FALSE);
			g_free(current_tag);
			break;
		}
	}
}

static gboolean config_init()
{
	gchar *user_config_dir;
	gboolean rcode = TRUE;
	
	default_config = g_build_path(G_DIR_SEPARATOR_S,
								  DHPLUG_DATA_DIR,
								  "devhelp.conf", 
								  NULL);
	
	user_config_dir = g_build_path(G_DIR_SEPARATOR_S,
							   geany_data->app->configdir,
							   "plugins",
							   "devhelp",
							   NULL);
							   
	user_config = g_build_path(G_DIR_SEPARATOR_S,
							   user_config_dir,
							   "devhelp.conf",
							   NULL);
	
	if (g_mkdir_with_parents(user_config_dir, S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
		g_warning(_("Unable to create config dir at '%s'"),
					user_config_dir);
		return FALSE;
	}
	
	/* copy default config into user config if it doesn't exist */
	if (!g_file_test(user_config, G_FILE_TEST_EXISTS))
	{
		gchar *config_text;
		GError *error;
		
		error = NULL;
		if (!g_file_get_contents(default_config, &config_text, NULL, &error))
		{
			g_warning(_("Unable to get default configuration: %s"), 
					  error->message);
			g_error_free(error);
			error = NULL;
			config_text = " ";
			rcode = FALSE;
		}
		if (!g_file_set_contents(user_config, config_text, -1, &error))
		{
			g_warning(_("Unable to write default configuration: %s"),
					  error->message);
			g_error_free(error);
			error = NULL;
			rcode = FALSE;
		}
	}
	
	return rcode;
}

void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *key_group;
	
	/* stop crashing when the plugin is re-loaded */
	plugin_module_make_resident(geany_plugin);

	config_init();				   
	
	sg_user = stash_group_new("general");
	stash_group_add_boolean(sg_user, 
							&move_sidebar_tabs_bottom, 
							"move_sidebar_tabs_bottom", 
							FALSE);
	
	if (!stash_group_load_from_file(sg_user, user_config))
		g_warning(_("Unable to load config file at '%s'"), user_config);
	
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
	if (stash_group_save_to_file(sg_user, user_config, G_KEY_FILE_NONE) != 0)
		g_error(_("Unable to save config file at '%s'"), user_config);
	
	stash_group_free(sg_user);
	
	if (dev_help_plugin != NULL)
		devhelp_plugin_destroy(dev_help_plugin);
	
	g_free(default_config);
	g_free(user_config);
}
