/*
 * Copyright (c) 2014-2025 gnome-mpv
 *
 * This file is part of Celluloid.
 *
 * Celluloid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Celluloid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Celluloid.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <adwaita.h>

#include "celluloid-preferences-dialog.h"
#include "celluloid-file-chooser-button.h"
#include "celluloid-plugins-manager.h"
#include "celluloid-main-window.h"
#include "celluloid-def.h"

typedef struct PreferencesDialogItem PreferencesDialogItem;
typedef enum PreferencesDialogItemType PreferencesDialogItemType;

enum
{
	PROP_0,
	PROP_PARENT,
	N_PROPERTIES
};

struct _CelluloidPreferencesDialog
{
	AdwPreferencesDialog parent_instance;
	GSettings *settings;
	gboolean needs_mpv_reset;
	GtkWindow *parent;
};

struct _CelluloidPreferencesDialogClass
{
	AdwPreferencesDialogClass parent_class;
};

enum PreferencesDialogItemType
{
	ITEM_TYPE_INVALID,
	ITEM_TYPE_SWITCH,
	ITEM_TYPE_FILE_CHOOSER,
	ITEM_TYPE_TEXT_BOX
};

struct PreferencesDialogItem
{
	const gchar *label;
	const gchar *key;
	PreferencesDialogItemType type;
};

G_DEFINE_TYPE(CelluloidPreferencesDialog, celluloid_preferences_dialog, ADW_TYPE_PREFERENCES_DIALOG)

static void
set_property(	GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec );

static void
get_property(	GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec );

static void
constructed(GObject *object);

static void
file_set_handler(CelluloidFileChooserButton *button, gpointer data);

static void
handle_changed(GSettings *settings, const gchar *key, gpointer data);

static void
handle_plugins_manager_error(	CelluloidPluginsManager *pmgr,
				const gchar *message,
				gpointer data );

static gboolean
save_settings(AdwPreferencesDialog *dialog);

static void
free_signal_data(gpointer data, GClosure *closure);

static GtkWidget *
build_page(	CelluloidPreferencesDialog *dialog,
		const PreferencesDialogItem *items,
		GSettings *settings,
		const char *title,
		const char *icon_name );

static void
finalize(GObject *object);

static void
set_property(	GObject *object,
		guint property_id,
		const GValue *value,
		GParamSpec *pspec )
{
	CelluloidPreferencesDialog *self = CELLULOID_PREFERENCES_DIALOG(object);

	switch(property_id)
	{
		case PROP_PARENT:
		self->parent = g_value_get_pointer(value);
		break;

		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
get_property(	GObject *object,
		guint property_id,
		GValue *value,
		GParamSpec *pspec )
{
	CelluloidPreferencesDialog *self = CELLULOID_PREFERENCES_DIALOG(object);

	switch(property_id)
	{
		case PROP_PARENT:
		g_value_set_pointer(value, self->parent);
		break;

		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
constructed(GObject *object)
{
	G_OBJECT_CLASS(celluloid_preferences_dialog_parent_class)->constructed(object);

	CelluloidPreferencesDialog *dlg = CELLULOID_PREFERENCES_DIALOG(object);

	const PreferencesDialogItem interface_items[]
		= {	{NULL,
			"autofit-enable",
			ITEM_TYPE_SWITCH},
			{NULL,
			"csd-enable",
			ITEM_TYPE_SWITCH},
			{NULL,
			"dark-theme-enable",
			ITEM_TYPE_SWITCH},
			{NULL,
			"show-durations-in-playlist",
			ITEM_TYPE_SWITCH},
			{NULL,
			"always-use-floating-controls",
			ITEM_TYPE_SWITCH},
			{NULL,
			"draggable-video-area-enable",
			ITEM_TYPE_SWITCH},
			{NULL,
			"always-autohide-cursor",
			ITEM_TYPE_SWITCH},
			{NULL,
			"last-folder-enable",
			ITEM_TYPE_SWITCH},
			{NULL, NULL, ITEM_TYPE_INVALID} };
	const PreferencesDialogItem config_items[]
		= {	{NULL,
			"mpv-config-enable",
			ITEM_TYPE_SWITCH},
			{_("mpv configuration file"),
			"mpv-config-file",
			ITEM_TYPE_FILE_CHOOSER},
			{NULL,
			"mpv-input-config-enable",
			ITEM_TYPE_SWITCH},
			{_("mpv input configuration file"),
			"mpv-input-config-file",
			ITEM_TYPE_FILE_CHOOSER},
			{NULL, NULL, ITEM_TYPE_INVALID} };
	const PreferencesDialogItem misc_items[]
		= {	{NULL,
			"always-open-new-window",
			ITEM_TYPE_SWITCH},
			{NULL,
			"always-append-to-playlist",
			ITEM_TYPE_SWITCH},
			{NULL,
			"ignore-playback-errors",
			ITEM_TYPE_SWITCH},
			{NULL,
			"prefetch-metadata",
			ITEM_TYPE_SWITCH},
			{NULL,
			"mpris-enable",
			ITEM_TYPE_SWITCH},
			{_("Extra mpv options"),
			"mpv-options",
			ITEM_TYPE_TEXT_BOX},
			{NULL, NULL, ITEM_TYPE_INVALID} };

	GtkWidget *page = NULL;

	page = build_page
		(	dlg,
			interface_items,
			dlg->settings,
			_("Interface"),
			"applications-graphics-symbolic" );
	adw_preferences_dialog_add(	ADW_PREFERENCES_DIALOG(dlg),
					ADW_PREFERENCES_PAGE(page));

	page = build_page
		(	dlg,
			config_items,
			dlg->settings,
			_("Config Files"),
			"document-properties-symbolic" );
	adw_preferences_dialog_add(	ADW_PREFERENCES_DIALOG(dlg),
					ADW_PREFERENCES_PAGE(page));

	page = build_page
		(	dlg,
			misc_items,
			dlg->settings,
			_("Miscellaneous"),
			"preferences-other-symbolic" );
	adw_preferences_dialog_add(	ADW_PREFERENCES_DIALOG(dlg),
					ADW_PREFERENCES_PAGE(page));

	AdwPreferencesPage *plugins_manager =
		celluloid_plugins_manager_new(dlg->parent);

	adw_preferences_dialog_add(	ADW_PREFERENCES_DIALOG(dlg),
					plugins_manager );

	g_signal_connect(	dlg,
				"closed",
				G_CALLBACK(save_settings),
				NULL );
	g_signal_connect(	dlg->settings,
				"changed",
				G_CALLBACK(handle_changed),
				dlg );
	g_signal_connect(	plugins_manager,
				"error-raised",
				G_CALLBACK(handle_plugins_manager_error),
				dlg );
}

static void
file_set_handler(CelluloidFileChooserButton *button, gpointer data)
{
	gpointer *signal_data = data;

	CelluloidPreferencesDialog *self =
		CELLULOID_PREFERENCES_DIALOG(signal_data[1]);
	const gchar *key =
		signal_data[0];

	GFile *file = celluloid_file_chooser_button_get_file(button);
	gchar *uri = g_file_get_uri(file) ?: g_strdup("");

	g_settings_set_string(self->settings, key, uri);

	g_free(uri);
	g_object_unref(file);
}

static void
handle_changed(GSettings *settings, const gchar *key, gpointer data)
{
	CelluloidPreferencesDialog *dlg = CELLULOID_PREFERENCES_DIALOG(data);

	dlg->needs_mpv_reset |= g_strcmp0(key, "mpv-config-enable") == 0;
	dlg->needs_mpv_reset |= g_strcmp0(key, "mpv-config-file") == 0;
	dlg->needs_mpv_reset |= g_strcmp0(key, "mpv-input-config-enable") == 0;
	dlg->needs_mpv_reset |= g_strcmp0(key, "mpv-input-config-file") == 0;
	dlg->needs_mpv_reset |= g_strcmp0(key, "mpv-options") == 0;
}

static void
handle_plugins_manager_error(	CelluloidPluginsManager *pmgr,
				const gchar *message,
				gpointer data )
{
	CelluloidPreferencesDialog *dialog =
		CELLULOID_PREFERENCES_DIALOG(data);

	g_signal_emit_by_name(dialog, "error-raised", message);
}

static gboolean
save_settings(AdwPreferencesDialog *dialog)
{
	CelluloidPreferencesDialog *dlg = CELLULOID_PREFERENCES_DIALOG(dialog);

	g_settings_apply(dlg->settings);

	if(dlg->needs_mpv_reset)
	{
		g_signal_emit_by_name(dlg, "mpv-reset-request");
		dlg->needs_mpv_reset = FALSE;
	}

	return FALSE;
}

static void
free_signal_data(gpointer data, GClosure *closure)
{
	gpointer *signal_data = data;
	g_free(signal_data[0]);
	g_object_unref(signal_data[1]);
	g_free(signal_data);
}

static GtkWidget *
build_page(	CelluloidPreferencesDialog *dialog,
		const PreferencesDialogItem *items,
		GSettings *settings,
		const char *title,
		const char *icon_name )
{
	GtkWidget *pref_page = adw_preferences_page_new();
	adw_preferences_page_set_title
		(ADW_PREFERENCES_PAGE(pref_page), title);
	adw_preferences_page_set_icon_name
		(ADW_PREFERENCES_PAGE(pref_page), icon_name);

	GtkWidget *pref_group = adw_preferences_group_new();
	adw_preferences_page_add
		(	ADW_PREFERENCES_PAGE(pref_page),
			ADW_PREFERENCES_GROUP(pref_group) );

	GSettingsSchema *schema = NULL;
	g_object_get(settings, "settings-schema", &schema, NULL);

	for(gint i = 0; items[i].type != ITEM_TYPE_INVALID; i++)
	{
		const gchar *key = items[i].key;
		GSettingsSchemaKey *schema_key =
			key ?
			g_settings_schema_get_key(schema, key) :
			NULL;
		const gchar *summary =
			schema_key ?
			g_settings_schema_key_get_summary(schema_key) :
			NULL;
		const gchar *label = items[i].label ?: summary;
		const PreferencesDialogItemType type = items[i].type;

		GtkWidget *widget = NULL;

		if(type == ITEM_TYPE_SWITCH)
		{
			GtkWidget *pref_switch;

			widget = adw_action_row_new();
			adw_preferences_row_set_title
				(ADW_PREFERENCES_ROW(widget), label);

			pref_switch = gtk_switch_new();
			gtk_widget_set_valign
				(pref_switch, GTK_ALIGN_CENTER);
			adw_action_row_add_suffix
				(ADW_ACTION_ROW(widget), pref_switch);
			adw_action_row_set_activatable_widget
				(ADW_ACTION_ROW(widget), pref_switch);

			g_settings_bind(	settings,
						key,
						pref_switch,
						"active",
						G_SETTINGS_BIND_DEFAULT );
		}
		else if(type == ITEM_TYPE_FILE_CHOOSER)
		{
			GtkWidget *button;
			GtkFileFilter *filter;
			gchar *uri;
			GFile *file;

			widget = adw_action_row_new();
			adw_preferences_row_set_title
				(ADW_PREFERENCES_ROW(widget), label);

			button = celluloid_file_chooser_button_new();
			gtk_widget_set_valign
				(button, GTK_ALIGN_CENTER);
			adw_action_row_add_suffix
				(ADW_ACTION_ROW(widget), button);
			adw_action_row_set_activatable_widget
				(ADW_ACTION_ROW(widget), button);

			filter = gtk_file_filter_new();
			uri = g_settings_get_string(settings, key);
			file = g_file_new_for_uri(uri);

			gtk_file_filter_add_mime_type
				(filter, "text/plain");
			celluloid_file_chooser_button_set_filter
				(CELLULOID_FILE_CHOOSER_BUTTON(button), filter);

			if(g_file_query_exists(file, NULL))
			{
				celluloid_file_chooser_button_set_file
					(	CELLULOID_FILE_CHOOSER_BUTTON(button),
						 file,
						 NULL );
			}

			gpointer *signal_data = g_malloc(2 * sizeof(gpointer));
			signal_data[0] = g_strdup(key);
			signal_data[1] = g_object_ref(dialog);

			/* For simplicity, changes made to the GSettings
			 * database externally won't be reflected immediately
			 * for this type of widget.
			 */
			g_signal_connect_data(	button,
						"file-set",
						G_CALLBACK(file_set_handler),
						signal_data,
						free_signal_data,
						0 );

			g_free(uri);
		}
		else if(type == ITEM_TYPE_TEXT_BOX)
		{

			widget = adw_entry_row_new();
			adw_preferences_row_set_title
				(ADW_PREFERENCES_ROW(widget), label);
			gtk_widget_set_hexpand
				(widget, TRUE);

			g_settings_bind(	settings,
						key,
						widget,
						"text",
						G_SETTINGS_BIND_DEFAULT );
		}

		adw_preferences_group_add
			(ADW_PREFERENCES_GROUP(pref_group), widget);
	}

	return pref_page;
}

static void
finalize(GObject *object)
{
	CelluloidPreferencesDialog *dialog = CELLULOID_PREFERENCES_DIALOG(object);

	g_clear_object(&dialog->settings);

	G_OBJECT_CLASS(celluloid_preferences_dialog_parent_class)
		->finalize(object);
}

static void
celluloid_preferences_dialog_class_init(CelluloidPreferencesDialogClass *klass)
{
	GParamSpec *pspec = NULL;
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->constructed = constructed;
	object_class->finalize = finalize;
	object_class->set_property = set_property;
	object_class->get_property = get_property;

	pspec = g_param_spec_pointer
		(	"parent",
			"Parent",
			"Parent window",
			G_PARAM_CONSTRUCT_ONLY|G_PARAM_READWRITE );
	g_object_class_install_property(object_class, PROP_PARENT, pspec);

	g_signal_new(	"mpv-reset-request",
			G_TYPE_FROM_CLASS(klass),
			G_SIGNAL_RUN_FIRST,
			0,
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0 );

	g_signal_new(	"error-raised",
			G_TYPE_FROM_CLASS(klass),
			G_SIGNAL_RUN_FIRST,
			0,
			NULL,
			NULL,
			g_cclosure_marshal_VOID__STRING,
			G_TYPE_NONE,
			1,
			G_TYPE_STRING );
}

static void
celluloid_preferences_dialog_init(CelluloidPreferencesDialog *dlg)
{
	dlg->settings = g_settings_new(CONFIG_ROOT);
	dlg->needs_mpv_reset = FALSE;
	dlg->parent = NULL;

	g_settings_delay(dlg->settings);
}

GtkWidget *
celluloid_preferences_dialog_new(GtkWindow *parent)
{
	GtkWidget *dlg;

	dlg = g_object_new(	celluloid_preferences_dialog_get_type(),
				"parent", parent,
				"title", _("Preferences"),
				NULL );

	return dlg;
}

void
celluloid_preferences_dialog_present(CelluloidPreferencesDialog *self)
{
	adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(self->parent));
}
