/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   peony-navigation-window-pane.c: Peony navigation window pane

   Copyright (C) 2008 Free Software Foundation, Inc.
   Copyright (C) 2017, Tianjin KYLIN Information Technology Co., Ltd.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: Holger Berndt <berndth@gmx.de>
           Zuxun Yang <yangzuxun@kylinos.cn>
*/

#include "peony-navigation-window-pane.h"
#include "peony-window-private.h"
#include "peony-window-manage-views.h"
#include "peony-pathbar.h"
#include "peony-location-bar.h"
#include "peony-notebook.h"
#include "peony-window-slot.h"

#include <libpeony-private/peony-global-preferences.h>
#include <libpeony-private/peony-window-slot-info.h>
#include <libpeony-private/peony-view-factory.h>
#include <libpeony-private/peony-entry.h>


static void peony_navigation_window_pane_init       (PeonyNavigationWindowPane *pane);
static void peony_navigation_window_pane_class_init (PeonyNavigationWindowPaneClass *class);
static void peony_navigation_window_pane_dispose    (GObject *object);

G_DEFINE_TYPE (PeonyNavigationWindowPane,
               peony_navigation_window_pane,
               PEONY_TYPE_WINDOW_PANE)
#define parent_class peony_navigation_window_pane_parent_class

static void
real_set_active (PeonyWindowPane *pane, gboolean is_active)
{
    PeonyNavigationWindowPane *nav_pane;
    GList *l;

    nav_pane = PEONY_NAVIGATION_WINDOW_PANE (pane);

    /* path bar */
    for (l = PEONY_PATH_BAR (nav_pane->path_bar)->button_list; l; l = l->next)
    {
        gtk_widget_set_sensitive (gtk_bin_get_child (GTK_BIN (peony_path_bar_get_button_from_button_list_entry (l->data))), is_active);
    }

    /* navigation bar (manual entry) */
    peony_location_bar_set_active (PEONY_LOCATION_BAR (nav_pane->navigation_bar), is_active);
    
    /* location button */
    gtk_widget_set_sensitive (gtk_bin_get_child (GTK_BIN (nav_pane->location_button)), is_active);
}

static gboolean
navigation_bar_focus_in_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
    PeonyWindowPane *pane;
    pane = PEONY_WINDOW_PANE (user_data);
    peony_window_set_active_pane (pane->window, pane);
    return FALSE;
}

static int
bookmark_list_get_uri_index (GList *list, GFile *location)
{
    PeonyBookmark *bookmark;
    GList *l;
    GFile *tmp;
    int i;

    g_return_val_if_fail (location != NULL, -1);

    for (i = 0, l = list; l != NULL; i++, l = l->next)
    {
        bookmark = PEONY_BOOKMARK (l->data);

        tmp = peony_bookmark_get_location (bookmark);
        if (g_file_equal (location, tmp))
        {
            g_object_unref (tmp);
            return i;
        }
        g_object_unref (tmp);
    }

    return -1;
}

static void
search_bar_focus_in_callback (PeonySearchBar *bar,
                              PeonyWindowPane *pane)
{
    peony_window_set_active_pane (pane->window, pane);
}


static void
search_bar_activate_callback (PeonySearchBar *bar,
                              PeonyNavigationWindowPane *pane)
{
    char *uri, *current_uri;
    PeonyDirectory *directory;
    PeonySearchDirectory *search_directory;
    PeonyQuery *query;
    GFile *location;

    uri = peony_search_directory_generate_new_uri ();
    location = g_file_new_for_uri (uri);
    g_free (uri);

    directory = peony_directory_get (location);

    g_assert (PEONY_IS_SEARCH_DIRECTORY (directory));

    search_directory = PEONY_SEARCH_DIRECTORY (directory);

    query = peony_search_bar_get_query (PEONY_SEARCH_BAR (pane->search_bar));
    if (query != NULL)
    {
        PeonyWindowSlot *slot = PEONY_WINDOW_PANE (pane)->active_slot;
        if (!peony_search_directory_is_indexed (search_directory))
        {
            current_uri = peony_window_slot_get_location_uri (slot);
            peony_query_set_location (query, current_uri);
            g_free (current_uri);
        }
        peony_search_directory_set_query (search_directory, query);
        g_object_unref (query);
    }

    peony_window_slot_go_to (PEONY_WINDOW_PANE (pane)->active_slot, location, FALSE);

    peony_directory_unref (directory);
    g_object_unref (location);
}

static void
search_bar_cancel_callback (GtkWidget *widget,
                            PeonyNavigationWindowPane *pane)
{
    if (peony_navigation_window_pane_hide_temporary_bars (pane))
    {
        peony_navigation_window_restore_focus_widget (PEONY_NAVIGATION_WINDOW (PEONY_WINDOW_PANE (pane)->window));
    }
}

static void
navigation_bar_cancel_callback (GtkWidget *widget,
                                PeonyNavigationWindowPane *pane)
{
    if (peony_navigation_window_pane_hide_temporary_bars (pane))
    {
        peony_navigation_window_restore_focus_widget (PEONY_NAVIGATION_WINDOW (PEONY_WINDOW_PANE (pane)->window));
    }
}

static void
navigation_bar_location_changed_callback (GtkWidget *widget,
        const char *uri,
        PeonyNavigationWindowPane *pane)
{
    GFile *location;

    if (peony_navigation_window_pane_hide_temporary_bars (pane))
    {
        peony_navigation_window_restore_focus_widget (PEONY_NAVIGATION_WINDOW (PEONY_WINDOW_PANE (pane)->window));
    }

    location = g_file_new_for_uri (uri);
    peony_window_slot_go_to (PEONY_WINDOW_PANE (pane)->active_slot, location, FALSE);
    g_object_unref (location);
}

static void
path_bar_location_changed_callback (GtkWidget *widget,
                                    GFile *location,
                                    PeonyNavigationWindowPane *pane)
{
    PeonyNavigationWindowSlot *slot;
    PeonyWindowPane *win_pane;
    int i;

    g_assert (PEONY_IS_NAVIGATION_WINDOW_PANE (pane));

    win_pane = PEONY_WINDOW_PANE(pane);

    slot = PEONY_NAVIGATION_WINDOW_SLOT (win_pane->active_slot);

    /* check whether we already visited the target location */
    i = bookmark_list_get_uri_index (slot->back_list, location);
    if (i >= 0)
    {
        peony_navigation_window_back_or_forward (PEONY_NAVIGATION_WINDOW (win_pane->window), TRUE, i, FALSE);
    }
    else
    {
        peony_window_slot_go_to (win_pane->active_slot, location, FALSE);
    }
}

static gboolean
location_button_should_be_active (PeonyNavigationWindowPane *pane)
{
    return g_settings_get_boolean (peony_preferences, PEONY_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);
}

static void
location_button_toggled_cb (GtkToggleButton *toggle,
                            PeonyNavigationWindowPane *pane)
{
    gboolean is_active;

    is_active = gtk_toggle_button_get_active (toggle);
    g_settings_set_boolean (peony_preferences, PEONY_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY, is_active);

    if (is_active) {
        peony_location_bar_activate (PEONY_LOCATION_BAR (pane->navigation_bar));
    }

    peony_window_set_active_pane (PEONY_WINDOW_PANE (pane)->window, PEONY_WINDOW_PANE (pane));
}

static GtkWidget *
location_button_create (PeonyNavigationWindowPane *pane)
{
    GtkWidget *image;
    GtkWidget *button;

    image = gtk_image_new_from_icon_name ("gtk-edit", GTK_ICON_SIZE_MENU);
    gtk_widget_show (image);

    button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                   "image", image,
                   "focus-on-click", FALSE,
                   "active", location_button_should_be_active (pane),
                   NULL);

    gtk_widget_set_tooltip_text (button,
                     _("Toggle between button and text-based location bar"));

    g_signal_connect (button, "toggled",
              G_CALLBACK (location_button_toggled_cb), pane);
    return button;
}

static gboolean
path_bar_button_pressed_callback (GtkWidget *widget,
                                  GdkEventButton *event,
                                  PeonyNavigationWindowPane *pane)
{
    PeonyWindowSlot *slot;
    PeonyView *view;
    GFile *location;
    char *uri;

    peony_window_set_active_pane (PEONY_WINDOW_PANE (pane)->window, PEONY_WINDOW_PANE (pane));

    g_object_set_data (G_OBJECT (widget), "handle-button-release",
                       GINT_TO_POINTER (TRUE));

    if (event->button == 3)
    {
        slot = peony_window_get_active_slot (PEONY_WINDOW_PANE (pane)->window);
        view = slot->content_view;
        if (view != NULL)
        {
            location = peony_path_bar_get_path_for_button (
                           PEONY_PATH_BAR (pane->path_bar), widget);
            if (location != NULL)
            {
                uri = g_file_get_uri (location);
                peony_view_pop_up_location_context_menu (
                    view, event, uri);
                g_object_unref (G_OBJECT (location));
                g_free (uri);
                return TRUE;
            }
        }
    }

    return FALSE;
}

static gboolean
path_bar_button_released_callback (GtkWidget *widget,
                                   GdkEventButton *event,
                                   PeonyNavigationWindowPane *pane)
{
    PeonyWindowSlot *slot;
    PeonyWindowOpenFlags flags;
    GFile *location;
    int mask;
    gboolean handle_button_release;

    mask = event->state & gtk_accelerator_get_default_mod_mask ();
    flags = 0;

    handle_button_release = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
                            "handle-button-release"));

    if (event->type == GDK_BUTTON_RELEASE && handle_button_release)
    {
        location = peony_path_bar_get_path_for_button (PEONY_PATH_BAR (pane->path_bar), widget);

        if (event->button == 2 && mask == 0)
        {
            flags = PEONY_WINDOW_OPEN_FLAG_NEW_TAB;
        }
        else if (event->button == 1 && mask == GDK_CONTROL_MASK)
        {
            flags = PEONY_WINDOW_OPEN_FLAG_NEW_WINDOW;
        }

        if (flags != 0)
        {
            slot = peony_window_get_active_slot (PEONY_WINDOW_PANE (pane)->window);
            peony_window_slot_info_open_location (slot, location,
                                                 PEONY_WINDOW_OPEN_ACCORDING_TO_MODE,
                                                 flags, NULL);
            g_object_unref (location);
            return TRUE;
        }

        g_object_unref (location);
    }

    return FALSE;
}

static void
path_bar_button_drag_begin_callback (GtkWidget *widget,
                                     GdkEventButton *event,
                                     gpointer user_data)
{
    g_object_set_data (G_OBJECT (widget), "handle-button-release",
                       GINT_TO_POINTER (FALSE));
}

static void
notebook_popup_menu_new_tab_cb (GtkMenuItem *menuitem,
    			gpointer user_data)
{
    PeonyWindowPane *pane;

    pane = PEONY_WINDOW_PANE (user_data);
    peony_window_new_tab (pane->window);
}

static void
path_bar_path_set_callback (GtkWidget *widget,
                            GFile *location,
                            PeonyNavigationWindowPane *pane)
{
    GList *children, *l;
    GtkWidget *child;

    children = gtk_container_get_children (GTK_CONTAINER (widget));

    for (l = children; l != NULL; l = l->next)
    {
        child = GTK_WIDGET (l->data);

        if (!GTK_IS_TOGGLE_BUTTON (child))
        {
            continue;
        }

        if (!g_signal_handler_find (child,
                                    G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                    0, 0, NULL,
                                    path_bar_button_pressed_callback,
                                    pane))
        {
            g_signal_connect (child, "button-press-event",
                              G_CALLBACK (path_bar_button_pressed_callback),
                              pane);
            g_signal_connect (child, "button-release-event",
                              G_CALLBACK (path_bar_button_released_callback),
                              pane);
            g_signal_connect (child, "drag-begin",
                              G_CALLBACK (path_bar_button_drag_begin_callback),
                              pane);
        }
    }

    g_list_free (children);
}

static void
notebook_popup_menu_move_left_cb (GtkMenuItem *menuitem,
                                  gpointer user_data)
{
    PeonyNavigationWindowPane *pane;

    pane = PEONY_NAVIGATION_WINDOW_PANE (user_data);
    peony_notebook_reorder_current_child_relative (PEONY_NOTEBOOK (pane->notebook), -1);
}

static void
notebook_popup_menu_move_right_cb (GtkMenuItem *menuitem,
                                   gpointer user_data)
{
    PeonyNavigationWindowPane *pane;

    pane = PEONY_NAVIGATION_WINDOW_PANE (user_data);
    peony_notebook_reorder_current_child_relative (PEONY_NOTEBOOK (pane->notebook), 1);
}

static void
notebook_popup_menu_close_cb (GtkMenuItem *menuitem,
                              gpointer user_data)
{
    PeonyWindowPane *pane;
    PeonyWindowSlot *slot;

    pane = PEONY_WINDOW_PANE (user_data);
    slot = pane->active_slot;
    peony_window_slot_close (slot);
}

static void
notebook_popup_menu_show (PeonyNavigationWindowPane *pane,
                          GdkEventButton *event)
{
    GtkWidget *popup;
    GtkWidget *item;
    GtkWidget *image;
    int button, event_time;
    gboolean can_move_left, can_move_right;
    PeonyNotebook *notebook;

    notebook = PEONY_NOTEBOOK (pane->notebook);

    can_move_left = peony_notebook_can_reorder_current_child_relative (notebook, -1);
    can_move_right = peony_notebook_can_reorder_current_child_relative (notebook, 1);

    popup = gtk_menu_new();

    item = gtk_menu_item_new_with_mnemonic (_("_New Tab"));
    g_signal_connect (item, "activate",
    		  G_CALLBACK (notebook_popup_menu_new_tab_cb),
    		  pane);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
    		       item);

    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
    		       gtk_separator_menu_item_new ());

    item = gtk_menu_item_new_with_mnemonic (_("Move Tab _Left"));
    g_signal_connect (item, "activate",
                      G_CALLBACK (notebook_popup_menu_move_left_cb),
                      pane);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           item);
    gtk_widget_set_sensitive (item, can_move_left);

    item = gtk_menu_item_new_with_mnemonic (_("Move Tab _Right"));
    g_signal_connect (item, "activate",
                      G_CALLBACK (notebook_popup_menu_move_right_cb),
                      pane);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           item);
    gtk_widget_set_sensitive (item, can_move_right);

    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           gtk_separator_menu_item_new ());

    item = gtk_image_menu_item_new_with_mnemonic (_("_Close Tab"));
    image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    g_signal_connect (item, "activate",
                      G_CALLBACK (notebook_popup_menu_close_cb), pane);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           item);

    gtk_widget_show_all (popup);

    if (event)
    {
        button = event->button;
        event_time = event->time;
    }
    else
    {
        button = 0;
        event_time = gtk_get_current_event_time ();
    }

    /* TODO is this correct? */
    gtk_menu_attach_to_widget (GTK_MENU (popup),
                               pane->notebook,
                               NULL);

    gtk_menu_popup (GTK_MENU (popup), NULL, NULL, NULL, NULL,
                    button, event_time);
}

/* emitted when the user clicks the "close" button of tabs */
static void
notebook_tab_close_requested (PeonyNotebook *notebook,
                              PeonyWindowSlot *slot,
                              PeonyWindowPane *pane)
{
    peony_window_pane_slot_close (pane, slot);
}

static gboolean
notebook_button_press_cb (GtkWidget *widget,
                          GdkEventButton *event,
                          gpointer user_data)
{
    PeonyNavigationWindowPane *pane;

    pane = PEONY_NAVIGATION_WINDOW_PANE (user_data);
    if (GDK_BUTTON_PRESS == event->type && 3 == event->button)
    {
        notebook_popup_menu_show (pane, event);
        return TRUE;
    }
    else if (GDK_BUTTON_PRESS == event->type && 2 == event->button)
    {
        PeonyWindowPane *wpane;
        PeonyWindowSlot *slot;

        wpane = PEONY_WINDOW_PANE (pane);
        slot = wpane->active_slot;
        peony_window_slot_close (slot);

        return FALSE;
    }

    return FALSE;
}

static gboolean
notebook_popup_menu_cb (GtkWidget *widget,
                        gpointer user_data)
{
    PeonyNavigationWindowPane *pane;

    pane = PEONY_NAVIGATION_WINDOW_PANE (user_data);
    notebook_popup_menu_show (pane, NULL);
    return TRUE;
}

static gboolean
notebook_switch_page_cb (GtkNotebook *notebook,
                         GtkWidget *page,
                         unsigned int page_num,
                         PeonyNavigationWindowPane *pane)
{
    PeonyWindowSlot *slot;
    GtkWidget *widget;

    widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (pane->notebook), page_num);
    g_assert (widget != NULL);

    /* find slot corresponding to the target page */
    slot = peony_window_pane_get_slot_for_content_box (PEONY_WINDOW_PANE (pane), widget);
    g_assert (slot != NULL);

    peony_window_set_active_slot (slot->pane->window, slot);
    
    peony_window_slot_update_icon (slot);

    return FALSE;
}

void
peony_navigation_window_pane_remove_page (PeonyNavigationWindowPane *pane, int page_num)
{
    GtkNotebook *notebook;
    notebook = GTK_NOTEBOOK (pane->notebook);

    g_signal_handlers_block_by_func (notebook,
                                     G_CALLBACK (notebook_switch_page_cb),
                                     pane);
    gtk_notebook_remove_page (notebook, page_num);
    g_signal_handlers_unblock_by_func (notebook,
                                       G_CALLBACK (notebook_switch_page_cb),
                                       pane);
}

void
peony_navigation_window_pane_add_slot_in_tab (PeonyNavigationWindowPane *pane, PeonyWindowSlot *slot, PeonyWindowOpenSlotFlags flags)
{
    PeonyNotebook *notebook;

    notebook = PEONY_NOTEBOOK (pane->notebook);
    g_signal_handlers_block_by_func (notebook,
                                     G_CALLBACK (notebook_switch_page_cb),
                                     pane);
    peony_notebook_add_tab (notebook,
                           slot,
                           (flags & PEONY_WINDOW_OPEN_SLOT_APPEND) != 0 ?
                           -1 :
                           gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook)) + 1,
                           FALSE);
    g_signal_handlers_unblock_by_func (notebook,
                                       G_CALLBACK (notebook_switch_page_cb),
                                       pane);
}

static void
real_sync_location_widgets (PeonyWindowPane *pane)
{
    PeonyNavigationWindowSlot *navigation_slot;
    PeonyNavigationWindowPane *navigation_pane;
    PeonyWindowSlot *slot;

    slot = pane->active_slot;
    navigation_pane = PEONY_NAVIGATION_WINDOW_PANE (pane);

    /* Change the location bar and path bar to match the current location. */
    if (slot->location != NULL)
    {
        char *uri;

        /* this may be NULL if we just created the slot */
        uri = peony_window_slot_get_location_uri (slot);
        peony_location_bar_set_location (PEONY_LOCATION_BAR (navigation_pane->navigation_bar), uri);
        g_free (uri);
        peony_path_bar_set_path (PEONY_PATH_BAR (navigation_pane->path_bar), slot->location);
    }

    /* Update window global UI if this is the active pane */
    if (pane == pane->window->details->active_pane)
    {
        peony_window_update_up_button (pane->window);

        /* Check if the back and forward buttons need enabling or disabling. */
        navigation_slot = PEONY_NAVIGATION_WINDOW_SLOT (pane->window->details->active_pane->active_slot);
        peony_navigation_window_allow_back (PEONY_NAVIGATION_WINDOW (pane->window),
                                           navigation_slot->back_list != NULL);
        peony_navigation_window_allow_forward (PEONY_NAVIGATION_WINDOW (pane->window),
                                              navigation_slot->forward_list != NULL);
    }
}

gboolean
peony_navigation_window_pane_hide_temporary_bars (PeonyNavigationWindowPane *pane)
{
    PeonyWindowSlot *slot;
    PeonyDirectory *directory;
    gboolean success;

    g_assert (PEONY_IS_NAVIGATION_WINDOW_PANE (pane));

    slot = PEONY_WINDOW_PANE(pane)->active_slot;
    success = FALSE;

    if (pane->temporary_location_bar)
    {
        if (peony_navigation_window_pane_location_bar_showing (pane))
        {
            peony_navigation_window_pane_hide_location_bar (pane, FALSE);
        }
        pane->temporary_location_bar = FALSE;
        success = TRUE;
    }
    if (pane->temporary_navigation_bar)
    {
        directory = peony_directory_get (slot->location);

        if (PEONY_IS_SEARCH_DIRECTORY (directory))
        {
            peony_navigation_window_pane_set_bar_mode (pane, PEONY_BAR_SEARCH);
        }
        else
        {
            if (!g_settings_get_boolean (peony_preferences, PEONY_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY))
            {
                peony_navigation_window_pane_set_bar_mode (pane, PEONY_BAR_PATH);
            }
        }
        pane->temporary_navigation_bar = FALSE;
        success = TRUE;

        peony_directory_unref (directory);
    }
    if (pane->temporary_search_bar)
    {
        PeonyNavigationWindow *window;

        if (!g_settings_get_boolean (peony_preferences, PEONY_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY))
        {
            peony_navigation_window_pane_set_bar_mode (pane, PEONY_BAR_PATH);
        }
        else
        {
            peony_navigation_window_pane_set_bar_mode (pane, PEONY_BAR_NAVIGATION);
        }
        window = PEONY_NAVIGATION_WINDOW (PEONY_WINDOW_PANE (pane)->window);
        peony_navigation_window_set_search_button (window, FALSE);
        pane->temporary_search_bar = FALSE;
        success = TRUE;
    }

    return success;
}

void
peony_navigation_window_pane_always_use_location_entry (PeonyNavigationWindowPane *pane, gboolean use_entry)
{
    if (use_entry)
    {
        peony_navigation_window_pane_set_bar_mode (pane, PEONY_BAR_NAVIGATION);
    }
    else
    {
        peony_navigation_window_pane_set_bar_mode (pane, PEONY_BAR_PATH);
    }
    
    g_signal_handlers_block_by_func (pane->location_button,
                                     G_CALLBACK (location_button_toggled_cb),
                                     pane);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pane->location_button), use_entry);
    g_signal_handlers_unblock_by_func (pane->location_button,
                                       G_CALLBACK (location_button_toggled_cb),
                                       pane);
}

void
peony_navigation_window_pane_setup (PeonyNavigationWindowPane *pane)
{
    GtkWidget *hbox;
    PeonyEntry *entry;
    GtkSizeGroup *header_size_group;
    GtkWidget *toolbar_table;	

    pane->widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    pane->location_bar = hbox;
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
    /*gtk_box_pack_start (GTK_BOX (pane->widget), hbox,
                        FALSE, FALSE, 0);
    gtk_widget_show (hbox);*/

    /* the header size group ensures that the location bar has the same height as the sidebar header */
    header_size_group = PEONY_NAVIGATION_WINDOW (PEONY_WINDOW_PANE (pane)->window)->details->header_size_group;
    gtk_size_group_add_widget (header_size_group, pane->location_bar);
    toolbar_table = PEONY_NAVIGATION_WINDOW(PEONY_WINDOW_PANE(pane)->window)->toolbar_table;

    pane->location_button = location_button_create (pane);
/*    gtk_box_pack_start (GTK_BOX (hbox), pane->location_button, FALSE, FALSE, 0);
    gtk_widget_show (pane->location_button);
*/
    pane->path_bar = g_object_new (PEONY_TYPE_PATH_BAR, NULL);
    gtk_widget_show (pane->path_bar);

    g_signal_connect_object (pane->path_bar, "path_clicked",
                             G_CALLBACK (path_bar_location_changed_callback), pane, 0);
    g_signal_connect_object (pane->path_bar, "path_set",
                             G_CALLBACK (path_bar_path_set_callback), pane, 0);

    /*gtk_box_pack_start (GTK_BOX (hbox),
                        pane->path_bar,
                        TRUE, TRUE, 0);*/

    pane->navigation_bar = peony_location_bar_new (pane);
    g_signal_connect_object (pane->navigation_bar, "location_changed",
                             G_CALLBACK (navigation_bar_location_changed_callback), pane, 0);
    g_signal_connect_object (pane->navigation_bar, "cancel",
                             G_CALLBACK (navigation_bar_cancel_callback), pane, 0);
    entry = peony_location_bar_get_entry (PEONY_LOCATION_BAR (pane->navigation_bar));
    g_signal_connect (entry, "focus-in-event",
                      G_CALLBACK (navigation_bar_focus_in_callback), pane);

    gtk_box_pack_start (GTK_BOX (hbox),
                        pane->navigation_bar,
                        TRUE, TRUE, 0);

    pane->search_bar = peony_search_bar_new ();
    g_signal_connect_object (pane->search_bar, "activate",
                             G_CALLBACK (search_bar_activate_callback), pane, 0);
    g_signal_connect_object (pane->search_bar, "cancel",
                             G_CALLBACK (search_bar_cancel_callback), pane, 0);
    g_signal_connect_object (pane->search_bar, "focus-in",
                             G_CALLBACK (search_bar_focus_in_callback), pane, 0);
    gtk_box_pack_start (GTK_BOX (hbox),
                        pane->search_bar,
                        FALSE, FALSE, 0);

    gtk_grid_attach(toolbar_table,
                   hbox,
                   1,1,1,1);
    gtk_widget_set_hexpand(hbox,TRUE);
    gtk_widget_show(pane->search_bar);
    pane->notebook = g_object_new (PEONY_TYPE_NOTEBOOK, NULL);
    gtk_box_pack_start (GTK_BOX (pane->widget), pane->notebook,
                        TRUE, TRUE, 0);
    g_signal_connect (pane->notebook,
                      "tab-close-request",
                      G_CALLBACK (notebook_tab_close_requested),
                      pane);
    g_signal_connect_after (pane->notebook,
                            "button_press_event",
                            G_CALLBACK (notebook_button_press_cb),
                            pane);
    g_signal_connect (pane->notebook, "popup-menu",
                      G_CALLBACK (notebook_popup_menu_cb),
                      pane);
    g_signal_connect (pane->notebook,
                      "switch-page",
                      G_CALLBACK (notebook_switch_page_cb),
                      pane);

    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (pane->notebook), FALSE);
    gtk_notebook_set_show_border (GTK_NOTEBOOK (pane->notebook), FALSE);
    gtk_widget_show (pane->notebook);
    gtk_container_set_border_width (GTK_CONTAINER (pane->notebook), 0);

    /* Ensure that the view has some minimal size and that other parts
     * of the UI (like location bar and tabs) don't request more and
     * thus affect the default position of the split view paned.
     */
    gtk_widget_set_size_request (pane->widget, 60, 60);
}


void
peony_navigation_window_pane_show_location_bar_temporarily (PeonyNavigationWindowPane *pane)
{
    if (!peony_navigation_window_pane_location_bar_showing (pane))
    {
        peony_navigation_window_pane_show_location_bar (pane, FALSE);
        pane->temporary_location_bar = TRUE;
    }
}

void
peony_navigation_window_pane_show_navigation_bar_temporarily (PeonyNavigationWindowPane *pane)
{
    if (peony_navigation_window_pane_path_bar_showing (pane)
            || peony_navigation_window_pane_search_bar_showing (pane))
    {
        peony_navigation_window_pane_set_bar_mode (pane, PEONY_BAR_NAVIGATION);
        pane->temporary_navigation_bar = TRUE;
    }
    peony_location_bar_activate
    (PEONY_LOCATION_BAR (pane->navigation_bar));
}

gboolean
peony_navigation_window_pane_path_bar_showing (PeonyNavigationWindowPane *pane)
{
    if (pane->path_bar != NULL)
    {
        return gtk_widget_get_visible (pane->path_bar);
    }
    /* If we're not visible yet we haven't changed visibility, so its TRUE */
    return TRUE;
}

void
peony_navigation_window_pane_set_bar_mode (PeonyNavigationWindowPane *pane,
        PeonyBarMode mode)
{
    gboolean use_entry;
    GtkWidget *focus_widget;
    PeonyNavigationWindow *window;

    switch (mode)
    {

    case PEONY_BAR_PATH:
        gtk_widget_show (pane->path_bar);
//        gtk_widget_hide (pane->navigation_bar);
  //      gtk_widget_hide (pane->search_bar);
        break;

    case PEONY_BAR_NAVIGATION:
        gtk_widget_show (pane->navigation_bar);
    //    gtk_widget_hide (pane->path_bar);
      //  gtk_widget_hide (pane->search_bar);
        break;

    case PEONY_BAR_SEARCH:
        gtk_widget_show (pane->search_bar);
//        gtk_widget_hide (pane->path_bar);
  //      gtk_widget_hide (pane->navigation_bar);
        break;
    }
    
    if (mode == PEONY_BAR_NAVIGATION || mode == PEONY_BAR_PATH) {
        use_entry = (mode == PEONY_BAR_NAVIGATION);

        g_signal_handlers_block_by_func (pane->location_button,
                         G_CALLBACK (location_button_toggled_cb),
                         pane);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pane->location_button),
                          use_entry);
        g_signal_handlers_unblock_by_func (pane->location_button,
                           G_CALLBACK (location_button_toggled_cb),
                           pane);
    }

    window = PEONY_NAVIGATION_WINDOW (PEONY_WINDOW_PANE (pane)->window);
    focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
    if (focus_widget != NULL && !peony_navigation_window_is_in_temporary_navigation_bar (focus_widget, window) &&
            !peony_navigation_window_is_in_temporary_search_bar (focus_widget, window))
    {
        if (mode == PEONY_BAR_NAVIGATION || mode == PEONY_BAR_PATH)
        {
            peony_navigation_window_set_search_button (window, FALSE);
        }
        else
        {
            peony_navigation_window_set_search_button (window, TRUE);
        }
    }
}

gboolean
peony_navigation_window_pane_search_bar_showing (PeonyNavigationWindowPane *pane)
{
    if (pane->search_bar != NULL)
    {
        return gtk_widget_get_visible (pane->search_bar);
    }
    /* If we're not visible yet we haven't changed visibility, so its TRUE */
    return TRUE;
}

void
peony_navigation_window_pane_hide_location_bar (PeonyNavigationWindowPane *pane, gboolean save_preference)
{
    pane->temporary_location_bar = FALSE;
    gtk_widget_hide(pane->location_bar);
    peony_navigation_window_update_show_hide_menu_items(
        PEONY_NAVIGATION_WINDOW (PEONY_WINDOW_PANE (pane)->window));
    if (save_preference)
    {
        g_settings_set_boolean (peony_window_state, PEONY_WINDOW_STATE_START_WITH_LOCATION_BAR, FALSE);
    }
}

void
peony_navigation_window_pane_show_location_bar (PeonyNavigationWindowPane *pane, gboolean save_preference)
{
    gtk_widget_show(pane->location_bar);
    peony_navigation_window_update_show_hide_menu_items(PEONY_NAVIGATION_WINDOW (PEONY_WINDOW_PANE (pane)->window));
    if (save_preference)
    {
        g_settings_set_boolean (peony_window_state, PEONY_WINDOW_STATE_START_WITH_LOCATION_BAR, TRUE);
    }
}

gboolean
peony_navigation_window_pane_location_bar_showing (PeonyNavigationWindowPane *pane)
{
    if (!PEONY_IS_NAVIGATION_WINDOW_PANE (pane))
    {
        return FALSE;
    }
    if (pane->location_bar != NULL)
    {
        return gtk_widget_get_visible (pane->location_bar);
    }
    /* If we're not visible yet we haven't changed visibility, so its TRUE */
    return TRUE;
}

static void
peony_navigation_window_pane_init (PeonyNavigationWindowPane *pane)
{
}

static void
peony_navigation_window_pane_show (PeonyWindowPane *pane)
{
    PeonyNavigationWindowPane *npane = PEONY_NAVIGATION_WINDOW_PANE (pane);

    gtk_widget_show (npane->widget);
}

/* either called due to slot change, or due to location change in the current slot. */
static void
real_sync_search_widgets (PeonyWindowPane *window_pane)
{
    PeonyWindowSlot *slot;
    PeonyDirectory *directory;
    PeonySearchDirectory *search_directory;
    PeonyNavigationWindowPane *pane;

    pane = PEONY_NAVIGATION_WINDOW_PANE (window_pane);
    slot = window_pane->active_slot;
    search_directory = NULL;

    directory = peony_directory_get (slot->location);
    if (PEONY_IS_SEARCH_DIRECTORY (directory))
    {
        search_directory = PEONY_SEARCH_DIRECTORY (directory);
    }

    if (search_directory != NULL &&
            !peony_search_directory_is_saved_search (search_directory))
    {
        peony_navigation_window_pane_show_location_bar_temporarily (pane);
        peony_navigation_window_pane_set_bar_mode (pane, PEONY_BAR_SEARCH);
        pane->temporary_search_bar = FALSE;
    }
    else
    {
        pane->temporary_search_bar = TRUE;
        peony_navigation_window_pane_hide_temporary_bars (pane);
    }
    peony_directory_unref (directory);
}

static void
peony_navigation_window_pane_class_init (PeonyNavigationWindowPaneClass *class)
{
    G_OBJECT_CLASS (class)->dispose = peony_navigation_window_pane_dispose;
    PEONY_WINDOW_PANE_CLASS (class)->show = peony_navigation_window_pane_show;
    PEONY_WINDOW_PANE_CLASS (class)->set_active = real_set_active;
    PEONY_WINDOW_PANE_CLASS (class)->sync_search_widgets = real_sync_search_widgets;
    PEONY_WINDOW_PANE_CLASS (class)->sync_location_widgets = real_sync_location_widgets;
}

static void
peony_navigation_window_pane_dispose (GObject *object)
{
    PeonyNavigationWindowPane *pane = PEONY_NAVIGATION_WINDOW_PANE (object);

    gtk_widget_destroy (pane->widget);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

PeonyNavigationWindowPane *
peony_navigation_window_pane_new (PeonyWindow *window)
{
    PeonyNavigationWindowPane *pane;

    pane = g_object_new (PEONY_TYPE_NAVIGATION_WINDOW_PANE, NULL);
    PEONY_WINDOW_PANE(pane)->window = window;

    return pane;
}