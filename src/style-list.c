/*
 * Copyright © 2012 Benjamin Otte
 * Copyright © 2012 Rui Matos
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "parasite.h"
#include "style-list.h"

enum {
  COLUMN_NAME,
  COLUMN_VALUE,
  COLUMN_LOCATION,
  N_COLUMNS
};

struct _ParasiteStyleListPrivate
{
  GHashTable *css_files;
  GtkListStore *model;
};

const char *known_properties[] = {
  "-adwaita-border-gradient",
  "-adwaita-focus-border-color",
  "-adwaita-focus-border-dashes",
  "-adwaita-focus-border-radius",
  "-adwaita-progressbar-pattern",
  "-adwaita-selected-tab-color",
  "background-clip",
  "background-color",
  "background-image",
  "background-origin",
  "background-repeat",
  "border-bottom-color",
  "border-bottom-left-radius",
  "border-bottom-right-radius",
  "border-bottom-style",
  "border-bottom-width",
  "border-image-repeat",
  "border-image-slice",
  "border-image-source",
  "border-image-width",
  "border-left-color",
  "border-left-style",
  "border-left-width",
  "border-right-color",
  "border-right-style",
  "border-right-width",
  "border-top-color",
  "border-top-left-radius",
  "border-top-right-radius",
  "border-top-style",
  "border-top-width",
  "box-shadow",
  "color",
  "engine",
  "font-family",
  "font-size",
  "font-style",
  "font-variant",
  "font-weight",
  "gtk-key-bindings",
  "icon-shadow",
  "margin-bottom",
  "margin-left",
  "margin-right",
  "margin-top",
  "outline-color",
  "outline-offset",
  "outline-style",
  "outline-width",
  "padding-bottom",
  "padding-left",
  "padding-right",
  "padding-top",
  "text-shadow",
  "transition"
};

G_DEFINE_TYPE (ParasiteStyleList, parasite_style_list, GTK_TYPE_TREE_VIEW)

static void
parasite_style_list_finalize (GObject *self)
{
  ParasiteStyleListPrivate *priv = PARASITE_STYLE_LIST (self)->priv;

  g_hash_table_destroy (priv->css_files);
  g_object_unref (priv->model);

  G_OBJECT_CLASS (parasite_style_list_parent_class)->finalize (self);
}

static void
parasite_style_list_class_init (ParasiteStyleListClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) class;

  object_class->finalize = parasite_style_list_finalize;

  g_type_class_add_private (class, sizeof (ParasiteStyleListPrivate));
}

static void
parasite_style_list_init (ParasiteStyleList *self)
{
  ParasiteStyleListPrivate *priv;
  GtkCellRenderer *renderer;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                   PARASITE_TYPE_STYLE_LIST,
                                                   ParasiteStyleListPrivate);

  priv->css_files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                           g_object_unref, (GDestroyNotify) g_strfreev);

  priv->model = gtk_list_store_new (N_COLUMNS,
                                    G_TYPE_STRING, /* NAME */
                                    G_TYPE_STRING, /* VALUE */
                                    G_TYPE_STRING); /* LOCATION */

  gtk_tree_view_set_model (GTK_TREE_VIEW (self),
                           GTK_TREE_MODEL (priv->model));

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "scale", TREE_TEXT_SCALE, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (self),
                                               -1,
                                               "Property", renderer,
                                               "text", COLUMN_NAME,
                                               NULL);
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "scale", TREE_TEXT_SCALE, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (self),
                                               -1,
                                               "Value", renderer,
                                               "text", COLUMN_VALUE,
                                               NULL);
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "scale", TREE_TEXT_SCALE, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (self),
                                               -1,
                                               "Location", renderer,
                                               "text", COLUMN_LOCATION,
                                               NULL);
}

GtkWidget *
parasite_style_list_new (void)
{
  return g_object_new (PARASITE_TYPE_STYLE_LIST, NULL);
}

static gchar *
strip_property (const gchar *property)
{
  gchar **split;
  gchar *value;

  split = g_strsplit_set (property, ":;", 3);
  if (!split[0] || !split[1])
    value = g_strdup ("");
  else
    value = g_strdup (split[1]);

  g_strfreev (split);

  return value;
}

static gchar *
parasite_style_get_css_content (ParasiteStyleList *self,
                                GFile             *file,
                                guint              start_line,
                                guint              end_line)
{
  ParasiteStyleListPrivate *priv = self->priv;
  guint i;
  guint contents_lines;
  gchar *value, *property;
  gchar **contents;

  contents = g_hash_table_lookup (priv->css_files, file);
  if (!contents)
    {
      gchar *tmp;

      if (g_file_load_contents (file, NULL, &tmp, NULL, NULL, NULL))
        {
          contents = g_strsplit_set (tmp, "\n\r", -1);
          g_free (tmp);
        }
      else
        {
          contents =  g_strsplit ("", "", -1);
        }

      g_object_ref (file);
      g_hash_table_insert (priv->css_files, file, contents);
    }

  contents_lines = g_strv_length (contents);

  property = g_strdup ("");
  for (i = start_line; (i < end_line + 1) && (i < contents_lines); ++i)
    {
      gchar *s1, *s2;

      s1 = g_strdup (contents[i]);
      s1 = g_strstrip (s1);
      s2 = g_strconcat (property, s1, NULL);
      g_free (property);
      g_free (s1);
      property = s2;
    }

  value = strip_property (property);
  g_free (property);

  return value;
}

static void
parasite_style_list_fill (ParasiteStyleList *self,
                          GtkWidget         *widget)
{
  ParasiteStyleListPrivate *priv = self->priv;
  GtkStyleContext *context;
  guint i;

  context = gtk_widget_get_style_context (widget);

  gtk_list_store_clear (priv->model);

  for (i = 0; i < G_N_ELEMENTS (known_properties); i++)
    {
      GtkCssSection *section;
      char *location;
      char *value;

      section = gtk_style_context_get_section (context, known_properties[i]);
      if (section)
        {
          GFileInfo *info;
          GFile *file;
          const char *path;
          guint start_line, end_line;

          start_line = gtk_css_section_get_start_line (section);
          end_line = gtk_css_section_get_end_line (section);

          file = gtk_css_section_get_file (section);
          if (file)
            {
              info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0, NULL, NULL);

              if (info)
                path = g_file_info_get_display_name (info);
              else
                path = "<broken file>";

              value = parasite_style_get_css_content (self, file, start_line, end_line);
            }
          else
            {
              info = NULL;
              path = "<data>";
              value = NULL;
            }

          if (end_line != start_line)
            {
              location = g_strdup_printf ("%s:%u-%u",
                                          path,
                                          start_line + 1,
                                          end_line + 1);
            }
          else
            {
              location = g_strdup_printf ("%s:%u",
                                          path,
                                          start_line + 1);
            }

          if (info)
            g_object_unref (info);
        }
      else
        {
          location = NULL;
          value = NULL;
        }

      gtk_list_store_insert_with_values (priv->model,
                                         NULL,
                                         -1,
                                         COLUMN_NAME, known_properties[i],
                                         COLUMN_VALUE, value,
                                         COLUMN_LOCATION, location,
                                         -1);

      g_free (location);
      g_free (value);
    }

  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (self));
}

static void
widget_style_updated (GtkWidget         *widget,
                      ParasiteStyleList *self)
{
  parasite_style_list_fill (self, widget);
}

static void
widget_state_flags_changed (GtkWidget         *widget,
                            GtkStateFlags      flags,
                            ParasiteStyleList *self)
{
  parasite_style_list_fill (self, widget);
}

static void
disconnect_each_other (gpointer  still_alive,
                       GObject  *for_science)
{
  g_signal_handlers_disconnect_matched (still_alive, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, for_science);
  g_object_weak_unref (still_alive, disconnect_each_other, for_science);
}

void
parasite_style_list_set_widget (ParasiteStyleList *self,
                                GtkWidget         *widget)
{
  g_return_if_fail (PARASITE_IS_STYLE_LIST (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  parasite_style_list_fill (self, widget);

  g_signal_connect (widget, "style-updated",
                    G_CALLBACK (widget_style_updated), self);
  g_signal_connect (widget, "state-flags-changed",
                    G_CALLBACK (widget_state_flags_changed), self);
  g_object_weak_ref (G_OBJECT (self), disconnect_each_other, widget);
  g_object_weak_ref (G_OBJECT (widget), disconnect_each_other, self);
}