/* ev-sidebar-attachments.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2006 Carlos Garcia Campos
 *
 * Author:
 *   Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>
#include <libgnomeui/gnome-icon-lookup.h>

#include "ev-sidebar-attachments.h"
#include "ev-sidebar-page.h"

enum {
	COLUMN_ICON,
	COLUMN_NAME,
	COLUMN_DESCRIPTION,
	COLUMN_ATTACHMENT,
	N_COLS
};


enum {
	PROP_0,
	PROP_WIDGET,
};

enum {
	SIGNAL_POPUP_MENU,
	N_SIGNALS
};

static const GtkTargetEntry drag_targets[] = {
	{ "text/uri-list", 0, 0 }
};

static guint signals[N_SIGNALS];

struct _EvSidebarAttachmentsPrivate {
	GtkWidget      *icon_view;
	GtkListStore   *model;

	/* Icons */
	GtkIconTheme   *icon_theme;
	GHashTable     *icon_cache;
};

static void ev_sidebar_attachments_page_iface_init (EvSidebarPageIface *iface);

G_DEFINE_TYPE_EXTENDED (EvSidebarAttachments,
                        ev_sidebar_attachments,
                        GTK_TYPE_VBOX,
                        0, 
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE, 
					       ev_sidebar_attachments_page_iface_init))

#define EV_SIDEBAR_ATTACHMENTS_GET_PRIVATE(object) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_ATTACHMENTS, EvSidebarAttachmentsPrivate))

/* Icon cache */
static void
ev_sidebar_attachments_icon_cache_add (EvSidebarAttachments *ev_attachbar,
				  const gchar     *mime_type,
				  const GdkPixbuf *pixbuf)
{
	g_assert (mime_type != NULL);
	g_assert (GDK_IS_PIXBUF (pixbuf));

	g_hash_table_insert (ev_attachbar->priv->icon_cache,
			     (gpointer)g_strdup (mime_type),
			     (gpointer)pixbuf);
			     
}

static GdkPixbuf *
icon_theme_get_pixbuf_from_mime_type (GtkIconTheme *icon_theme,
				      const gchar  *mime_type)
{
	GdkPixbuf *pixbuf = NULL;
	gchar     *icon;

	icon = gnome_icon_lookup (icon_theme,
				  NULL, NULL,
				  NULL, NULL,
				  mime_type,
				  GNOME_ICON_LOOKUP_FLAGS_NONE,
				  NULL);

	pixbuf = gtk_icon_theme_load_icon (icon_theme,
					   icon, 48, 0, NULL);
	g_free (icon);

	return pixbuf;
}

static GdkPixbuf *
ev_sidebar_attachments_icon_cache_get (EvSidebarAttachments *ev_attachbar,
				  const gchar     *mime_type)
{
	GdkPixbuf *pixbuf = NULL;
	
	g_assert (mime_type != NULL);

	pixbuf = g_hash_table_lookup (ev_attachbar->priv->icon_cache,
				      mime_type);

	if (GDK_IS_PIXBUF (pixbuf))
		return pixbuf;

	pixbuf = icon_theme_get_pixbuf_from_mime_type (ev_attachbar->priv->icon_theme,
						       mime_type);

	if (GDK_IS_PIXBUF (pixbuf))
		ev_sidebar_attachments_icon_cache_add (ev_attachbar,
						  mime_type,
						  pixbuf);

	return pixbuf;
}

static gboolean
icon_cache_update_icon (gchar           *key,
			GdkPixbuf       *value,
			EvSidebarAttachments *ev_attachbar)
{
	GdkPixbuf *pixbuf = NULL;

	pixbuf = icon_theme_get_pixbuf_from_mime_type (ev_attachbar->priv->icon_theme,
						       key);

	ev_sidebar_attachments_icon_cache_add (ev_attachbar,
					  key,
					  pixbuf);
	
	return FALSE;
}

static void
ev_sidebar_attachments_icon_cache_refresh (EvSidebarAttachments *ev_attachbar)
{
	g_hash_table_foreach_remove (ev_attachbar->priv->icon_cache,
				     (GHRFunc) icon_cache_update_icon,
				     ev_attachbar);
}

static EvAttachment *
ev_sidebar_attachments_get_attachment_at_pos (EvSidebarAttachments *ev_attachbar,
					 gint             x,
					 gint             y)
{
	GtkTreePath  *path = NULL;
	GtkTreeIter   iter;
	EvAttachment *attachment = NULL;

	path = gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (ev_attachbar->priv->icon_view),
					      x, y);
	if (!path) {
		return NULL;
	}

	gtk_tree_model_get_iter (GTK_TREE_MODEL (ev_attachbar->priv->model),
				 &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (ev_attachbar->priv->model), &iter,
			    COLUMN_ATTACHMENT, &attachment,
			    -1);

	gtk_icon_view_select_path (GTK_ICON_VIEW (ev_attachbar->priv->icon_view),
				   path);
	
	gtk_tree_path_free (path);

	return attachment;
}

static gboolean
ev_sidebar_attachments_popup_menu_show (EvSidebarAttachments *ev_attachbar,
				   gint             x,
				   gint             y)
{
	GtkIconView *icon_view;
	GtkTreePath *path;
	GList       *selected = NULL, *l;
	GList       *attach_list = NULL;

	icon_view = GTK_ICON_VIEW (ev_attachbar->priv->icon_view);
	
	path = gtk_icon_view_get_path_at_pos (icon_view, x, y);
	if (!path)
		return FALSE;

	if (!gtk_icon_view_path_is_selected (icon_view, path)) {
		gtk_icon_view_unselect_all (icon_view);
		gtk_icon_view_select_path (icon_view, path);
	}

	gtk_tree_path_free (path);
	
	selected = gtk_icon_view_get_selected_items (icon_view);
	if (!selected)
		return FALSE;

	for (l = selected; l && l->data; l = g_list_next (l)) {
		GtkTreeIter   iter;
		EvAttachment *attachment = NULL;

		path = (GtkTreePath *) l->data;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (ev_attachbar->priv->model),
					 &iter, path);
		gtk_tree_model_get (GTK_TREE_MODEL (ev_attachbar->priv->model), &iter,
				    COLUMN_ATTACHMENT, &attachment,
				    -1);

		if (attachment)
			attach_list = g_list_prepend (attach_list, attachment);

		gtk_tree_path_free (path);
	}

	g_list_free (selected);

	if (!attach_list)
		return FALSE;

	g_signal_emit (ev_attachbar, signals[SIGNAL_POPUP_MENU], 0, attach_list);

	return TRUE;
}

static gboolean
ev_sidebar_attachments_popup_menu (GtkWidget *widget)
{
	EvSidebarAttachments *ev_attachbar = EV_SIDEBAR_ATTACHMENTS (widget);
	gint             x, y;

	gtk_widget_get_pointer (widget, &x, &y);

	return ev_sidebar_attachments_popup_menu_show (ev_attachbar, x, y);
}

static gboolean
ev_sidebar_attachments_button_press (EvSidebarAttachments *ev_attachbar,
				GdkEventButton  *event,
				GtkWidget       *icon_view)
{
	if (!GTK_WIDGET_HAS_FOCUS (icon_view)) {
		gtk_widget_grab_focus (icon_view);
	}
	
	if (event->button == 2)
		return FALSE;

	switch (event->button) {
	case 1:
		if (event->type == GDK_2BUTTON_PRESS) {
			GError *error = NULL;
			EvAttachment *attachment;

			attachment = ev_sidebar_attachments_get_attachment_at_pos (ev_attachbar,
									      event->x,
									      event->y);
			if (!attachment)
				return FALSE;
			
			ev_attachment_open (attachment, &error);

			if (error) {
				g_warning (error->message);
				g_error_free (error);
			}

			g_object_unref (attachment);
					    
			return TRUE;
		}
		break;
	case 3: 
		return ev_sidebar_attachments_popup_menu_show (ev_attachbar, event->x, event->y);
	}

	return FALSE;
}

static void
ev_sidebar_attachments_update_icons (EvSidebarAttachments *ev_attachbar,
				gpointer         user_data)
{
	GtkTreeIter iter;
	gboolean    valid;

	ev_sidebar_attachments_icon_cache_refresh (ev_attachbar);
	
	valid = gtk_tree_model_get_iter_first (
		GTK_TREE_MODEL (ev_attachbar->priv->model),
		&iter);

	while (valid) {
		EvAttachment *attachment = NULL;
		GdkPixbuf    *pixbuf = NULL;
		const gchar  *mime_type;

		gtk_tree_model_get (GTK_TREE_MODEL (ev_attachbar->priv->model), &iter,
				    COLUMN_ATTACHMENT, &attachment,
				    -1);

		mime_type = ev_attachment_get_mime_type (attachment);

		if (attachment)
			g_object_unref (attachment);

		pixbuf = ev_sidebar_attachments_icon_cache_get (ev_attachbar,
							   mime_type);

		gtk_list_store_set (ev_attachbar->priv->model, &iter,
				    COLUMN_ICON, pixbuf,
				    -1);

		valid = gtk_tree_model_iter_next (
			GTK_TREE_MODEL (ev_attachbar->priv->model),
			&iter);
	}
}

static void
ev_sidebar_attachments_drag_data_get (GtkWidget        *widget,
				 GdkDragContext   *drag_context,
				 GtkSelectionData *data,
				 guint             info,
				 guint             time,
				 gpointer          user_data)
{
	EvSidebarAttachments *ev_attachbar = EV_SIDEBAR_ATTACHMENTS (user_data);
	GString         *uri_list;
	gchar           *uris = NULL;
	GList           *selected = NULL, *l;

	selected = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (ev_attachbar->priv->icon_view));
	if (!selected)
		return;

	uri_list = g_string_new (NULL);
	
	for (l = selected; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;
		GtkTreePath  *path;
		GtkTreeIter   iter;
		gchar        *uri, *filename;
		GError       *error = NULL;
		
		path = (GtkTreePath *) l->data;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (ev_attachbar->priv->model),
					 &iter, path);
		gtk_tree_model_get (GTK_TREE_MODEL (ev_attachbar->priv->model), &iter,
				    COLUMN_ATTACHMENT, &attachment,
				    -1);

		filename = g_build_filename (g_get_tmp_dir (),
					     ev_attachment_get_name (attachment),
					     NULL);
		
		uri = g_filename_to_uri (filename, NULL, NULL);

		if (ev_attachment_save (attachment, filename, &error)) {
			g_string_append (uri_list, uri);
			g_string_append_c (uri_list, '\n');
		}
	
		if (error) {
			g_warning (error->message);
			g_error_free (error);
		}

		g_free (uri);
		gtk_tree_path_free (path);
		g_object_unref (attachment);
	}

	uris = g_string_free (uri_list, FALSE);

	if (uris) {
		gtk_selection_data_set (data,
					data->target,
					8,
					(guchar *)uris,
					strlen (uris));
	}

	g_list_free (selected);
}

static void
ev_sidebar_attachments_get_property (GObject    *object,
				     guint       prop_id,
			    	     GValue     *value,
		      	             GParamSpec *pspec)
{
	EvSidebarAttachments *ev_sidebar_attachments;
  
	ev_sidebar_attachments = EV_SIDEBAR_ATTACHMENTS (object);

	switch (prop_id)
	{
	case PROP_WIDGET:
		g_value_set_object (value, ev_sidebar_attachments->priv->icon_view);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_sidebar_attachments_destroy (GtkObject *object)
{
	EvSidebarAttachments *ev_attachbar = EV_SIDEBAR_ATTACHMENTS (object);

	if (ev_attachbar->priv->model) {
		g_object_unref (ev_attachbar->priv->model);
		ev_attachbar->priv->model = NULL;
	}

	if (ev_attachbar->priv->icon_cache) {
		g_hash_table_destroy (ev_attachbar->priv->icon_cache);
		ev_attachbar->priv->icon_cache = NULL;
	}

	(* GTK_OBJECT_CLASS (ev_sidebar_attachments_parent_class)->destroy) (object);
}

static void
ev_sidebar_attachments_class_init (EvSidebarAttachmentsClass *ev_attachbar_class)
{
	GObjectClass   *g_object_class;
	GtkObjectClass *gtk_object_class;
	GtkWidgetClass *gtk_widget_class;

	g_object_class = G_OBJECT_CLASS (ev_attachbar_class);
	gtk_object_class = GTK_OBJECT_CLASS (ev_attachbar_class);
	gtk_widget_class = GTK_WIDGET_CLASS (ev_attachbar_class);

	g_object_class->get_property = ev_sidebar_attachments_get_property;
	gtk_object_class->destroy = ev_sidebar_attachments_destroy;
	gtk_widget_class->popup_menu = ev_sidebar_attachments_popup_menu;

	g_type_class_add_private (g_object_class, sizeof (EvSidebarAttachmentsPrivate));

	/* Signals */
	signals[SIGNAL_POPUP_MENU] =
		g_signal_new ("popup",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvSidebarAttachmentsClass, popup_menu),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	g_object_class_override_property (g_object_class,
					  PROP_WIDGET,
					  "main-widget");
}

static void
ev_sidebar_attachments_init (EvSidebarAttachments *ev_attachbar)
{
	GtkWidget *swindow;
	
	ev_attachbar->priv = EV_SIDEBAR_ATTACHMENTS_GET_PRIVATE (ev_attachbar);

	swindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					     GTK_SHADOW_IN);
	/* Data Model */
	ev_attachbar->priv->model = gtk_list_store_new (N_COLS,
							GDK_TYPE_PIXBUF, 
							G_TYPE_STRING,  
							G_TYPE_STRING,
							EV_TYPE_ATTACHMENT);

	/* Icon View */
	ev_attachbar->priv->icon_view =
		gtk_icon_view_new_with_model (GTK_TREE_MODEL (ev_attachbar->priv->model));
	gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (ev_attachbar->priv->icon_view),
					  GTK_SELECTION_MULTIPLE);
	gtk_icon_view_set_columns (GTK_ICON_VIEW (ev_attachbar->priv->icon_view), -1);
	g_object_set (G_OBJECT (ev_attachbar->priv->icon_view),
		      "text-column", COLUMN_NAME,
		      "pixbuf-column", COLUMN_ICON,
		      NULL);
	g_signal_connect_swapped (G_OBJECT (ev_attachbar->priv->icon_view),
				  "button-press-event",
				  G_CALLBACK (ev_sidebar_attachments_button_press),
				  (gpointer) ev_attachbar);

	gtk_container_add (GTK_CONTAINER (swindow),
			   ev_attachbar->priv->icon_view);

	gtk_container_add (GTK_CONTAINER (ev_attachbar),
			   swindow);
	gtk_widget_show_all (GTK_WIDGET (ev_attachbar));
	/* Icon Theme */
	ev_attachbar->priv->icon_theme = gtk_icon_theme_get_default ();
	g_signal_connect_swapped (G_OBJECT (ev_attachbar->priv->icon_theme),
				  "changed",
				  G_CALLBACK (ev_sidebar_attachments_update_icons),
				  (gpointer) ev_attachbar);

	/* Icon Cache */
	ev_attachbar->priv->icon_cache = g_hash_table_new_full (g_str_hash,
								g_str_equal,
								g_free,
								g_object_unref);

	/* Drag and Drop */
#ifdef HAVE_GTK_ICON_VIEW_ENABLE_MODEL_DRAG_SOURCE
	gtk_icon_view_enable_model_drag_source (
		GTK_ICON_VIEW (ev_attachbar->priv->icon_view),
			       GDK_BUTTON1_MASK,
			       drag_targets,
			       G_N_ELEMENTS (drag_targets),
			       GDK_ACTION_COPY);
#endif
	g_signal_connect (G_OBJECT (ev_attachbar->priv->icon_view),
			  "drag-data-get",
			  G_CALLBACK (ev_sidebar_attachments_drag_data_get),
			  (gpointer) ev_attachbar);	
}

GtkWidget *
ev_sidebar_attachments_new (void)
{
	GtkWidget *ev_attachbar;

	ev_attachbar = g_object_new (EV_TYPE_SIDEBAR_ATTACHMENTS, NULL);

	return ev_attachbar;
}

static void
ev_sidebar_attachments_set_document (EvSidebarPage   *page,
				     EvDocument      *document)
{
	EvSidebarAttachments *ev_attachbar = EV_SIDEBAR_ATTACHMENTS (page);
	GList *attachments = NULL;
	GList *l;
	
	if (!ev_document_has_attachments (document))
		return;

	attachments = ev_document_get_attachments (document);

	gtk_list_store_clear (ev_attachbar->priv->model);
					   
	for (l = attachments; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;
		GtkTreeIter   iter;
		GdkPixbuf    *pixbuf = NULL;
		const gchar  *mime_type;

		attachment = EV_ATTACHMENT (l->data);

		mime_type = ev_attachment_get_mime_type (attachment);
		pixbuf = ev_sidebar_attachments_icon_cache_get (ev_attachbar,
							   mime_type);

		gtk_list_store_append (ev_attachbar->priv->model, &iter);
		gtk_list_store_set (ev_attachbar->priv->model, &iter,
				    COLUMN_NAME, ev_attachment_get_name (attachment),
				    COLUMN_ICON, pixbuf,
				    COLUMN_ATTACHMENT, attachment, 
				    -1);

		g_object_unref (attachment);
	}

	g_list_free (attachments);
}

static gboolean
ev_sidebar_attachments_support_document (EvSidebarPage   *sidebar_page,
				        EvDocument *document)
{
	return ev_document_has_attachments (document);
}

static const gchar*
ev_sidebar_attachments_get_label (EvSidebarPage *sidebar_page)
{
    return _("Attachments");
}

static void
ev_sidebar_attachments_page_iface_init (EvSidebarPageIface *iface)
{
	iface->support_document = ev_sidebar_attachments_support_document;
	iface->set_document = ev_sidebar_attachments_set_document;
	iface->get_label = ev_sidebar_attachments_get_label;
}
