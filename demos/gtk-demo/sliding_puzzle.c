/* Sliding puzzle
 *
 * This demo demonstrates how to use gestures and paintables to create a
 * small sliding puzzle game.
 *
 */

#include <gtk/gtk.h>

/* Include the header for the puzzle piece */
#include "puzzlepiece.h"
#include "paintable.h"

#define ICON_TYPE_PAINTABLE (icon_paintable_get_type ())

G_DECLARE_FINAL_TYPE (IconPaintable, icon_paintable, ICON, PAINTABLE, GObject)

GdkPaintable *  icon_paintable_new                  (GdkPaintable *paintable,
                                                     double        height);

struct _IconPaintable
{
  GObject parent_instance;

  GdkPaintable *paintable;
  double height;
  double scale_factor;
};

struct _IconPaintableClass
{
  GObjectClass parent_class;
};


static void
icon_paintable_paintable_snapshot (GdkPaintable *paintable,
                                   GdkSnapshot  *snapshot,
                                   double        width,
                                   double        height)
{
  IconPaintable *self = ICON_PAINTABLE (paintable);

  if (self->scale_factor == 1.0)
    {
      gdk_paintable_snapshot (self->paintable, snapshot, width, height);
    }
  else
    {
      graphene_matrix_t scale_matrix;

      graphene_matrix_init_scale (&scale_matrix, 1.0 / self->scale_factor, 1.0 / self->scale_factor, 1.0);
      gtk_snapshot_push_transform (snapshot, &scale_matrix);
      gdk_paintable_snapshot (self->paintable,
                              snapshot,
                              width * self->scale_factor,
                              height * self->scale_factor);
      gtk_snapshot_pop (snapshot);
    }
}

static GdkPaintable *
icon_paintable_paintable_get_current_image (GdkPaintable *paintable)
{
  IconPaintable *self = ICON_PAINTABLE (paintable);
  GdkPaintable *current_paintable, *current_self;

  current_paintable = gdk_paintable_get_current_image (self->paintable);
  current_self = icon_paintable_new (current_paintable, self->height);
  g_object_unref (current_paintable);

  return current_self;
}

static GdkPaintableFlags
icon_paintable_paintable_get_flags (GdkPaintable *paintable)
{
  IconPaintable *self = ICON_PAINTABLE (paintable);

  return gdk_paintable_get_flags (self->paintable);
}

static int
icon_paintable_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  IconPaintable *self = ICON_PAINTABLE (paintable);

  return gdk_paintable_get_intrinsic_width (self->paintable) / self->scale_factor;
}

static int
icon_paintable_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  IconPaintable *self = ICON_PAINTABLE (paintable);

  return gdk_paintable_get_intrinsic_height (self->paintable) / self->scale_factor;
}

static double icon_paintable_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  IconPaintable *self = ICON_PAINTABLE (paintable);

  return gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);
};

static void
icon_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = icon_paintable_paintable_snapshot;
  iface->get_current_image = icon_paintable_paintable_get_current_image;
  iface->get_flags = icon_paintable_paintable_get_flags;
  iface->get_intrinsic_width = icon_paintable_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = icon_paintable_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = icon_paintable_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_EXTENDED (IconPaintable, icon_paintable, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               icon_paintable_paintable_init))

static void
icon_paintable_dispose (GObject *object)
{
  IconPaintable *self = ICON_PAINTABLE (object);

  if (self->paintable)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_size, self);
      g_clear_object (&self->paintable);
    }

  G_OBJECT_CLASS (icon_paintable_parent_class)->dispose (object);
}

static void
icon_paintable_class_init (IconPaintableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = icon_paintable_dispose;
}

static void
icon_paintable_init (IconPaintable *self)
{
  self->scale_factor = 1.0;
}

GdkPaintable *
icon_paintable_new (GdkPaintable *paintable,
                    double        height)
{
  IconPaintable *self;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), NULL);
  g_return_val_if_fail (height > 0.0, NULL);

  self = g_object_new (ICON_TYPE_PAINTABLE, NULL);

  self->paintable = g_object_ref (paintable);
  g_signal_connect_swapped (paintable, "invalidate-contents", G_CALLBACK (gdk_paintable_invalidate_contents), self);
  g_signal_connect_swapped (paintable, "invalidate-size", G_CALLBACK (gdk_paintable_invalidate_size), self);
  self->height = height;
  if (gdk_paintable_get_intrinsic_height (paintable) == 0)
    self->scale_factor = 1.0;
  else
    self->scale_factor = gdk_paintable_get_intrinsic_height (paintable) / height;

  return GDK_PAINTABLE (self);
}


static GtkWidget *window = NULL;
static GtkWidget *frame = NULL;
static GtkWidget *size_spin = NULL;
static GtkWidget *rose_button = NULL;
static GdkPaintable *puzzle = NULL;
static GdkPaintable *rose = NULL;
static GdkPaintable *atom = NULL;

static gboolean solved = TRUE;
static guint width = 3;
static guint height = 3;
static guint pos_x;
static guint pos_y;

static gboolean
move_puzzle (GtkWidget *grid,
             int        dx,
             int        dy)
{
  GtkWidget *pos, *next;
  GdkPaintable *piece;
  guint next_x, next_y;

  /* We don't move anything if the puzzle is solved */
  if (solved)
    return FALSE;

  /* Return FALSE if we can't move to where the call 
   * wants us to move.
   */
  if ((dx < 0 && pos_x < -dx) ||
      dx + pos_x >= width ||
      (dy < 0 && pos_y < -dy) ||
      dy + pos_y >= height)
    return FALSE;

  /* Compute the new position */
  next_x = pos_x + dx;
  next_y = pos_y + dy;

  /* Get the current and next image */
  pos = gtk_grid_get_child_at (GTK_GRID (grid), pos_x, pos_y);
  next = gtk_grid_get_child_at (GTK_GRID (grid), next_x, next_y);

  /* Move the displayed piece. */
  piece = gtk_image_get_paintable (GTK_IMAGE (next));
  gtk_image_set_from_paintable (GTK_IMAGE (pos), piece);
  gtk_image_clear (GTK_IMAGE (next));

  /* Update the current position */
  pos_x = next_x;
  pos_y = next_y;

  /* Return TRUE because we successfully moved the piece */
  return TRUE;
}

static void
shuffle_puzzle (GtkWidget *grid)
{
  guint i, n_steps;

  /* Do this many random moves */
  n_steps = width * height * 50;

  for (i = 0; i < n_steps; i++)
    {
      /* Get a random number for the direction to move in */
      switch (g_random_int_range (0, 4))
        {
        case 0:
          /* left */
          move_puzzle (grid, -1, 0);
          break;

        case 1:
          /* up */
          move_puzzle (grid, 0, -1);
          break;

        case 2:
          /* right */
          move_puzzle (grid, 1, 0);
          break;

        case 3:
          /* down */
          move_puzzle (grid, 0, 1);
          break;

        default:
          g_assert_not_reached ();
          continue;
        }
    }
}

static gboolean
check_solved (GtkWidget *grid)
{
  GtkWidget *image;
  GdkPaintable *piece;
  guint x, y;

  /* Nothing to check if the puzzle is already solved */
  if (solved)
    return TRUE;

  /* If the empty cell isn't in the bottom right,
   * the puzzle is obviously not solved */
  if (pos_x != width - 1 ||
      pos_y != height - 1)
    return FALSE;

  /* Check that all pieces are in the right position */
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          image = gtk_grid_get_child_at (GTK_GRID (grid), x, y);
          piece = gtk_image_get_paintable (GTK_IMAGE (image));

          /* empty cell */
          if (piece == NULL)
            continue;

          if (gtk_puzzle_piece_get_x (GTK_PUZZLE_PIECE (piece)) != x ||
              gtk_puzzle_piece_get_y (GTK_PUZZLE_PIECE (piece)) != y)
            return FALSE;
        }
    }

  /* We solved the puzzle!
   */
  solved = TRUE;

  /* Fill the empty cell to show that we're done.
   */
  image = gtk_grid_get_child_at (GTK_GRID (grid), 0, 0);
  piece = gtk_image_get_paintable (GTK_IMAGE (image));

  piece = gtk_puzzle_piece_new (gtk_puzzle_piece_get_puzzle (GTK_PUZZLE_PIECE (piece)),
                                pos_x, pos_y,
                                width, height);
  image = gtk_grid_get_child_at (GTK_GRID (grid), pos_x, pos_y);
  gtk_image_set_from_paintable (GTK_IMAGE (image), piece);

  return TRUE;
}

static gboolean
puzzle_key_pressed (GtkEventControllerKey *controller,
                    guint                  keyval,
                    guint                  keycode,
                    GdkModifierType        state,
                    GtkWidget             *grid)
{
  int dx, dy;

  dx = 0;
  dy = 0;

  switch (keyval)
    {
    case GDK_KEY_KP_Left:
    case GDK_KEY_Left:
      /* left */
      dx = -1;
      break;

    case GDK_KEY_KP_Up:
    case GDK_KEY_Up:
      /* up */
      dy = -1;
      break;

    case GDK_KEY_KP_Right:
    case GDK_KEY_Right:
      /* right */
      dx = 1;
      break;

    case GDK_KEY_KP_Down:
    case GDK_KEY_Down:
      /* down */
      dy = 1;
      break;

    default:
      /* We return FALSE here because we didn't handle the key that was pressed */
      return FALSE;
    }

  if (!move_puzzle (grid, dx, dy))
    {
      /* Make the error sound and then return TRUE.
       * We handled this key, even though we didn't
       * do anything to the puzzle.
       */
      gtk_widget_error_bell (grid);
      return TRUE;
    }

  check_solved (grid);

  return TRUE;
}

static void
start_puzzle (GdkPaintable *puzzle)
{
  GtkWidget *image, *grid;
  GtkEventController *controller;
  guint x, y;

  /* Remove the old grid (if there is one) */
  grid = gtk_bin_get_child (GTK_BIN (frame));
  if (grid)
    gtk_container_remove (GTK_CONTAINER (frame), grid);

  /* Create a new grid */
  grid = gtk_grid_new ();
  gtk_widget_set_can_focus (grid, TRUE);
  gtk_container_add (GTK_CONTAINER (frame), grid);

  /* Add a key event controller so people can use the arrow
   * keys to move the puzzle */
  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (puzzle_key_pressed),
                    grid);
  gtk_widget_add_controller (GTK_WIDGET (grid), controller);

  /* Make sure the cells have equal size */
  gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);

  /* Reset the variables */
  solved = FALSE;
  pos_x = width - 1;
  pos_y = height - 1;

  /* add an image for every cell */
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          GdkPaintable *piece;

          /* Don't paint anything for the lsiding part of the video */
          if (x == pos_x && y == pos_y)
            piece = NULL;
          else
            piece = gtk_puzzle_piece_new (puzzle,
                                          x, y,
                                          width, height);
          image = gtk_image_new_from_paintable (piece);
          gtk_image_set_keep_aspect_ratio (GTK_IMAGE (image), FALSE);
          gtk_image_set_can_shrink (GTK_IMAGE (image), TRUE);
          gtk_grid_attach (GTK_GRID (grid),
                           image,
                           x, y,
                           1, 1);
        }
    }

  shuffle_puzzle (grid);
}

static void
reshuffle (void)
{
  GtkWidget *grid;

  grid = gtk_bin_get_child (GTK_BIN (frame));
  if (solved)
    start_puzzle (puzzle);
  else
    shuffle_puzzle (grid);
  gtk_widget_grab_focus (grid);
}

static void
reconfigure (void)
{
  GtkWidget *popover;
  GtkWidget *grid;

  width = height = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (size_spin));
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rose_button)))
    puzzle = rose;
  else
    puzzle= atom;

  start_puzzle (puzzle); 
  popover = gtk_widget_get_ancestor (size_spin, GTK_TYPE_POPOVER);
  gtk_popover_popdown (GTK_POPOVER (popover));
  grid = gtk_bin_get_child (GTK_BIN (frame));
  gtk_widget_grab_focus (grid);
}

GtkWidget *
do_sliding_puzzle (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *header;
      GtkWidget *restart;
      GtkWidget *tweak;
      GtkWidget *popover;
      GtkWidget *tweaks;
      GtkWidget *apply;
      GtkWidget *image;
      GtkWidget *button;
      GtkWidget *label;

      rose = GDK_PAINTABLE (gdk_texture_new_from_resource ("/sliding_puzzle/portland-rose.jpg"));
      atom = gtk_nuclear_animation_new ();

      puzzle = rose;

      tweaks = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (tweaks), 10);
      gtk_grid_set_column_spacing (GTK_GRID (tweaks), 10);
      g_object_set (tweaks, "margin", 10, NULL);

      rose_button = button = gtk_radio_button_new (NULL);
      image = gtk_image_new_from_paintable (icon_paintable_new (rose, 40));
      gtk_image_set_can_shrink (GTK_IMAGE (image), TRUE);
      gtk_widget_set_size_request (image, 40, 40);
      gtk_container_add (GTK_CONTAINER (button), image);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      gtk_grid_attach (GTK_GRID (tweaks), button, 0, 0, 1, 1);

      button = gtk_radio_button_new_from_widget (button);
      image = gtk_image_new_from_paintable (icon_paintable_new (atom, 40));
      gtk_image_set_can_shrink (GTK_IMAGE (image), TRUE);
      gtk_widget_set_size_request (image, 40, 40);
      gtk_container_add (GTK_CONTAINER (button), image);
      gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      gtk_grid_attach (GTK_GRID (tweaks), button, 1, 0, 1, 1);

      label = gtk_label_new ("Size");
      gtk_label_set_xalign (label, 0.0);
      gtk_grid_attach (GTK_GRID (tweaks), label, 0, 1, 1, 1);
      size_spin = gtk_spin_button_new_with_range (2, 10, 1);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (size_spin), width);
      gtk_grid_attach (GTK_GRID (tweaks), size_spin, 1, 1, 1, 1);

      apply = gtk_button_new_with_label ("Apply");
      gtk_widget_set_halign (apply, GTK_ALIGN_END);
      gtk_grid_attach (GTK_GRID (tweaks), apply, 1, 2, 1, 1);
      g_signal_connect (apply, "clicked", G_CALLBACK (reconfigure), NULL);
      popover = gtk_popover_new (NULL);
      gtk_popover_set_modal (GTK_POPOVER (popover), TRUE);
      gtk_container_add (GTK_CONTAINER (popover), tweaks);
      restart = gtk_button_new_from_icon_name ("view-refresh-symbolic");
      g_signal_connect (restart, "clicked", G_CALLBACK (reshuffle), NULL);
      tweak = gtk_menu_button_new ();
      gtk_menu_button_set_popover (GTK_MENU_BUTTON (tweak), popover);
      gtk_button_set_icon_name (tweak, "emblem-system-symbolic");

      header = gtk_header_bar_new ();
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), restart);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), tweak);
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Sliding Puzzle");
      gtk_window_set_titlebar (GTK_WINDOW (window), header);
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      frame = gtk_aspect_frame_new (NULL, 0.5, 0.5, (float) gdk_paintable_get_intrinsic_aspect_ratio (puzzle), FALSE);
      gtk_container_add (GTK_CONTAINER (window), frame);

      start_puzzle (puzzle);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}