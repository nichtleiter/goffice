/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * go-3d-rotation-sel.c: A widget to select rotation angles of 3d plot
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA.
 */
#include <goffice/goffice-config.h>
#include "go-3d-rotation-sel.h"

#include <goffice/gtk/goffice-gtk.h>
#include <goffice/math/go-math.h>
#include <goffice/cut-n-paste/foocanvas/foo-canvas.h>
#include <goffice/cut-n-paste/foocanvas/foo-canvas-util.h>
#include <goffice/cut-n-paste/foocanvas/foo-canvas-line.h>
#include <goffice/cut-n-paste/foocanvas/foo-canvas-widget.h>
#include <goffice/cut-n-paste/foocanvas/foo-canvas-rect-ellipse.h>
#include <goffice/cut-n-paste/foocanvas/foo-canvas-polygon.h>
#include <gsf/gsf-impl-utils.h>
#include <glib/gi18n-lib.h>
#include <string.h>

typedef struct {
	double x, y, z;
} g3d_point;

struct _GO3DRotationSel {
	GtkHBox		 box;
	GladeXML	*gui;
	GtkRange	*fovscale;
	double		 psi;
	double		 theta;
	double		 phi;
	double		 fov;
	GOMatrix3x3	 mat;

	int		 radius;
	int		 margin;
	int		 bank_dial_x;
	int		 bank_dial_y;
	int		 bank_dial_r;
	double		 old_x, old_y;
	double		 bank;
	g3d_point	*cube_points;
	int 	 	*cube_faces;
	FooCanvas       *rotate_canvas;
	FooCanvasItem   *dial;
	FooCanvasItem	*bank_dial;
	FooCanvasItem	*cube_polygons[6];
	gulong		 motion_handle;
};

typedef struct {
	GtkHBoxClass parent_class;
	void (* psi_changed) (GO3DRotationSel *g3d, int angle);
	void (* theta_changed) (GO3DRotationSel *g3d, int angle);
	void (* phi_changed) (GO3DRotationSel *g3d, int angle);
	void (* fov_changed) (GO3DRotationSel *g3d, int angle);
} GO3DRotationSelClass;

enum {
	MATRIX_CHANGED,
	PSI_CHANGED,
	THETA_CHANGED,
	PHI_CHANGED,
	FOV_CHANGED,
	LAST_SIGNAL
};

static guint g3d_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0 };
static GObjectClass *g3d_parent_class;

/* NOTE: This works only for angles [-2*pi,4*pi] */
static double
g3d_normalize (double angle)
{
	if (angle > 2 * M_PI)
		return angle - 2 * M_PI;
	if (angle < 0)
		return angle + 2 * M_PI;
	return angle;
}

static void
cb_rotation_changed (GO3DRotationSel *g3d)
{
	double mgn = g3d->margin - 2;
	double r = g3d->radius;
	double d = 2 * r;
	double dr = g3d->bank_dial_r;
	double dx = g3d->bank_dial_x = mgn + r * (1 - sin (g3d->bank));
	double dy = g3d->bank_dial_y = mgn + r * (1 - cos (g3d->bank));

	/*double psi, theta, phi;*/
	g3d_point cp[] = {
		{ 50,  50,  50},
		{ 50, -50,  50},
		{-50, -50,  50},
		{-50,  50,  50},
		{ 50,  50, -50},
		{ 50, -50, -50},
		{-50, -50, -50},
		{-50,  50, -50},
	};
	const int cf[] = {
		0, 1, 2, 3,
		4, 5, 6, 7,
		0, 1, 5, 4,
		1, 2, 6, 5,
		2, 3, 7, 6,
		3, 0, 4, 7
	};
	int i;

	if (g3d->dial) {
		foo_canvas_item_set (g3d->dial,
			"x1", mgn, "y1", mgn,
			"x2", mgn + d, "y2", mgn + d, NULL);
	}

	if (g3d->bank_dial) {
		foo_canvas_item_set (g3d->bank_dial,
			"x1", dx - dr, "y1", dy - dr,
			"x2", dx + dr, "y2", dy + dr, NULL);
	}

	for (i = 0; i < 8; ++i) {
		double x = cp[i].x;
		double y = cp[i].y;
		double z = cp[i].z;
		go_matrix3x3_transform (&g3d->mat, x, y, z,
		                        &cp[i].x, &cp[i].y, &cp[i].z);
	}

	for (i = 0; i < 6; ++i) {
		FooCanvasPoints *points;
		double cx = mgn + r;
		double mean_y;
		if (g3d->cube_polygons[i] == NULL)
			continue;

		points = foo_canvas_points_new (5);
		points->coords[0] = cp[cf[4 * i + 0]].x + cx;
		points->coords[1] = -cp[cf[4 * i + 0]].z + cx;
		points->coords[2] = cp[cf[4 * i + 1]].x + cx;
		points->coords[3] = -cp[cf[4 * i + 1]].z + cx;
		points->coords[4] = cp[cf[4 * i + 2]].x + cx;
		points->coords[5] = -cp[cf[4 * i + 2]].z + cx;
		points->coords[6] = cp[cf[4 * i + 3]].x + cx;
		points->coords[7] = -cp[cf[4 * i + 3]].z + cx;
		points->coords[8] = cp[cf[4 * i + 0]].x + cx;
		points->coords[9] = -cp[cf[4 * i + 0]].z + cx;

		/* NOTE: This back face culling method works only with
		 * a cube in parallel projection*/
		mean_y  = cp[cf[4 * i + 0]].y;
		mean_y += cp[cf[4 * i + 1]].y;
		mean_y += cp[cf[4 * i + 2]].y;
		mean_y += cp[cf[4 * i + 3]].y;
		foo_canvas_item_set (g3d->cube_polygons[i], "points", points,
		                     "width_units", (mean_y < 0) ? 4. : 0.5, 
				     "fill-color", (i == 1)? "light blue" : "none",
				     NULL);
		foo_canvas_points_free (points);
	}
	go_matrix3x3_to_euler (&g3d->mat, &g3d->psi, &g3d->theta, &g3d->phi);

	g3d->psi = g3d_normalize (g3d->psi);
	g3d->theta = g3d_normalize (g3d->theta);
	g3d->phi = g3d_normalize (g3d->phi);

	g_signal_emit (G_OBJECT (g3d),
		g3d_signals[PSI_CHANGED], 0, (int) (g3d->psi * 180 / M_PI));
	g_signal_emit (G_OBJECT (g3d),
		g3d_signals[THETA_CHANGED], 0, (int) (g3d->theta * 180 / M_PI));
	g_signal_emit (G_OBJECT (g3d),
		g3d_signals[PHI_CHANGED], 0, (int) (g3d->phi * 180 / M_PI));
}

static gboolean
cb_fov_changed (GtkRange *range, GdkEventButton *event, GO3DRotationSel *g3d)
{
	int angle = gtk_range_get_value (GTK_RANGE (range));
	g3d->fov = angle * M_PI / 180;
	g_signal_emit (G_OBJECT (g3d), g3d_signals[FOV_CHANGED], 0, angle);
	return FALSE;
}

static void
cb_rotate_canvas_realize (FooCanvas *canvas, GO3DRotationSel *g3d)
{
	FooCanvasGroup  *group = FOO_CANVAS_GROUP (foo_canvas_root (canvas));
	GtkStyle *style = gtk_style_copy (GTK_WIDGET (canvas)->style);
	int i;
	style->bg[GTK_STATE_NORMAL] = style->white;
	gtk_widget_set_style (GTK_WIDGET (canvas), style);
	g_object_unref (style);

	foo_canvas_set_scroll_region (canvas, 0, 0, 220, 220);
	foo_canvas_scroll_to (canvas, 0, 0);

	for (i = 0; i < 6; ++i) {
		g3d->cube_polygons[i] = foo_canvas_item_new (group,
			FOO_TYPE_CANVAS_POLYGON, "outline-color", "black",
			NULL);
	}

	g3d->dial = foo_canvas_item_new (group,
        	FOO_TYPE_CANVAS_ELLIPSE, "outline_color", "black",
		"width_units", 2., NULL);

	g3d->bank_dial = foo_canvas_item_new (group,
		FOO_TYPE_CANVAS_ELLIPSE, "outline_color", "black",
		"fill_color", "white", "width_units", 3., NULL);

	cb_rotation_changed(g3d);	
}


static gboolean
cb_bank_dial_motion_notify_event (FooCanvas *canvas, GdkEventMotion *event,
			          GO3DRotationSel *g3d)
{
	GOMatrix3x3 m1, m2;
	double x = event->x;
	double y = event->y;
	double theta, bank;

	x -= g3d->margin + g3d->radius;
	y -= g3d->margin + g3d->radius;

	bank = g3d_normalize (-atan2 (x, -y));
	theta = g3d->bank - bank;
	g3d->bank = bank;
	g3d->old_x = event->x;
	g3d->old_y = event->y;
	go_matrix3x3_from_euler (&m1, 0, theta, -M_PI * 0.5);
	go_matrix3x3_from_euler (&m2, 0, 0, M_PI * 0.5);
	go_matrix3x3_multiply (&m1, &m2, &m1);
	go_matrix3x3_multiply (&g3d->mat, &m1, &g3d->mat);
	cb_rotation_changed (g3d);
	return TRUE;
}

static gboolean
cb_rotate_motion_notify_event (FooCanvas *canvas, GdkEventMotion *event,
			       GO3DRotationSel *g3d)
{
	GOMatrix3x3 m1, m2, m3;
	double theta, phi, dx, dy;

	dx = event->x - g3d->old_x;
	dy = event->y - g3d->old_y;
	theta = atan2 (dy, dx);
	phi = sqrt (dx * dx + dy * dy) * M_PI / 180;
	g3d->old_x = event->x;
	g3d->old_y = event->y;

	go_matrix3x3_from_euler (&m1, 0, theta, phi);
	go_matrix3x3_from_euler_transposed (&m2, 0, theta, 0);
	go_matrix3x3_from_euler (&m3, 0, 0, M_PI * 0.5);
	go_matrix3x3_multiply (&m1, &m3, &m1);
	go_matrix3x3_multiply (&m1, &m1, &m2);
	go_matrix3x3_from_euler (&m3, 0, 0, -M_PI * 0.5);
	go_matrix3x3_multiply (&m1, &m1, &m3);
	go_matrix3x3_multiply (&g3d->mat, &m1, &g3d->mat);
	cb_rotation_changed (g3d);	
	return TRUE;
}

static gboolean
cb_rotate_canvas_button (FooCanvas *canvas, GdkEventButton *event,
                         GO3DRotationSel *g3d)
{
	double x, y, r;
	if (event->type == GDK_BUTTON_PRESS) {
		if (g3d->motion_handle != 0)
			return TRUE;
		x = g3d->old_x = event->x;
		y = g3d->old_y = event->y;
		x -= g3d->bank_dial_x;
		y -= g3d->bank_dial_y;
		r = g3d->bank_dial_r;
		gdk_pointer_grab (canvas->layout.bin_window, FALSE,
			GDK_POINTER_MOTION_MASK
			| GDK_BUTTON_RELEASE_MASK, NULL, NULL,
			event->time);

		if (x * x + y * y <= r * r) {
			g3d->motion_handle = g_signal_connect (G_OBJECT (canvas),
				"motion_notify_event",
				G_CALLBACK (cb_bank_dial_motion_notify_event),
				g3d);
		} else {
			g3d->motion_handle = g_signal_connect (G_OBJECT (canvas),
				"motion_notify_event",
				G_CALLBACK (cb_rotate_motion_notify_event), g3d);
		}
	} else if (event->type == GDK_BUTTON_RELEASE) {
		if (g3d->motion_handle == 0)
			return TRUE;
		gdk_display_pointer_ungrab (gtk_widget_get_display (GTK_WIDGET (canvas)),
			event->time);
		g_signal_handler_disconnect (canvas, g3d->motion_handle);
		g3d->motion_handle = 0;
		g_signal_emit (G_OBJECT (g3d),
			g3d_signals[MATRIX_CHANGED], 0, &g3d->mat);
	}
	return TRUE;
}

static void
g3d_init (GO3DRotationSel *g3d)
{
	GtkWidget *w;

	g3d->gui = go_glade_new ("go-3d-rotation-sel.glade", "toplevel",
		GETTEXT_PACKAGE, NULL);
	if (g3d->gui == NULL)
		return;

	g3d->radius = 100;
	g3d->margin = 10;
	g3d->bank_dial_x = g3d->margin + g3d->radius;
	g3d->bank_dial_y = g3d->margin;
	g3d->bank_dial_r = 7;
	g3d->dial = NULL;
	g3d->bank_dial = NULL;
	memset (g3d->cube_polygons, 0, sizeof (g3d->cube_polygons));
	g3d->rotate_canvas = FOO_CANVAS (foo_canvas_new ());
	gtk_container_add (GTK_CONTAINER (glade_xml_get_widget (g3d->gui,
	                   "rotate_canvas")),
	                   GTK_WIDGET (g3d->rotate_canvas));
	gtk_widget_show (GTK_WIDGET (g3d->rotate_canvas));
	
	g3d->motion_handle = 0;
	g_object_connect (G_OBJECT (g3d->rotate_canvas),
	                  "signal::realize",
			  G_CALLBACK (cb_rotate_canvas_realize), g3d,
	                  "signal::button-press-event",
			  G_CALLBACK (cb_rotate_canvas_button), g3d,
	                  "signal::button-release-event",
			  G_CALLBACK (cb_rotate_canvas_button), g3d,
	                  NULL);

	g3d->fovscale = GTK_RANGE (glade_xml_get_widget (g3d->gui, "fovscale"));
	g_object_connect (G_OBJECT (g3d->fovscale),
	                  "signal::button-release-event",
	                  G_CALLBACK (cb_fov_changed), g3d,
			  "signal::key-release-event",
			  G_CALLBACK (cb_fov_changed), g3d,
			  NULL);
	w = glade_xml_get_widget (g3d->gui, "toplevel");
	gtk_box_pack_start (GTK_BOX (g3d), w, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (g3d));
}

static void
g3d_finalize (GObject *obj)
{
	GO3DRotationSel *g3d = GO_3D_ROTATION_SEL (obj);

	if (g3d->gui) {
		g_object_unref (G_OBJECT (g3d->gui));
		g3d->gui = NULL;
	}

	g3d_parent_class->finalize (obj);
}

static void
g3d_class_init (GObjectClass *klass)
{
	GObjectClass *gobj_class = (GObjectClass *) klass;
	gobj_class->finalize = g3d_finalize;

	g3d_parent_class = g_type_class_peek (gtk_hbox_get_type ());
	g3d_signals [MATRIX_CHANGED] = g_signal_new ("matrix-changed",
		G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GO3DRotationSelClass, psi_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	g3d_signals [PSI_CHANGED] = g_signal_new ("psi-changed",
		G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GO3DRotationSelClass, psi_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);
	g3d_signals [THETA_CHANGED] = g_signal_new ("theta-changed",
		G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GO3DRotationSelClass, theta_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);
	g3d_signals [PHI_CHANGED] = g_signal_new ("phi-changed",
		G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GO3DRotationSelClass, phi_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);
	g3d_signals [FOV_CHANGED] = g_signal_new ("fov-changed",
		G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GO3DRotationSelClass, fov_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);
}

GSF_CLASS (GO3DRotationSel, go_3d_rotation_sel,
	   g3d_class_init, g3d_init, GTK_TYPE_HBOX)

GtkWidget *
go_3d_rotation_sel_new (void)
{
	return g_object_new (GO_3D_ROTATION_SEL_TYPE, NULL);
}

void
go_3d_rotation_sel_get_matrix (GO3DRotationSel const *g3d, GOMatrix3x3 *mat)
{
	mat->a11 = g3d->mat.a11;
	mat->a12 = g3d->mat.a12;
	mat->a13 = g3d->mat.a13;
	mat->a21 = g3d->mat.a21;
	mat->a22 = g3d->mat.a22;
	mat->a23 = g3d->mat.a23;
	mat->a31 = g3d->mat.a31;
	mat->a32 = g3d->mat.a32;
	mat->a33 = g3d->mat.a33;
}

double
go_3d_rotation_sel_get_psi (GO3DRotationSel const *g3d)
{
	g_return_val_if_fail (GO_IS_3D_ROTATION_SEL (g3d), 0);
	return g3d->psi;
}

double
go_3d_rotation_sel_get_theta (GO3DRotationSel const *g3d)
{
	g_return_val_if_fail (GO_IS_3D_ROTATION_SEL (g3d), 0);
	return g3d->theta;
}

double
go_3d_rotation_sel_get_phi (GO3DRotationSel const *g3d)
{
	g_return_val_if_fail (GO_IS_3D_ROTATION_SEL (g3d), 0);
	return g3d->phi;
}

double
go_3d_rotation_sel_get_fov (GO3DRotationSel const *g3d)
{
	g_return_val_if_fail (GO_IS_3D_ROTATION_SEL (g3d), 0);
	return g3d->fov;
}

void
go_3d_rotation_sel_set_matrix (GO3DRotationSel *g3d, GOMatrix3x3 *mat)
{
	g3d->mat.a11 = mat->a11; 
	g3d->mat.a12 = mat->a12; 
	g3d->mat.a13 = mat->a13; 
	g3d->mat.a21 = mat->a21; 
	g3d->mat.a22 = mat->a22; 
	g3d->mat.a23 = mat->a23; 
	g3d->mat.a31 = mat->a31; 
	g3d->mat.a32 = mat->a32; 
	g3d->mat.a33 = mat->a33; 

	cb_rotation_changed (g3d);
}

void
go_3d_rotation_sel_set_fov (GO3DRotationSel *g3d, double fov)
{
	g3d->fov = fov;
	g_signal_handlers_block_matched (GTK_RANGE (g3d->fovscale),
		G_SIGNAL_MATCH_FUNC, 0, 0, 0, G_CALLBACK (cb_fov_changed), 0);
	gtk_range_set_value (GTK_RANGE (g3d->fovscale),
		(int) (fov * 180. / M_PI));
	g_signal_handlers_unblock_matched (GTK_RANGE (g3d->fovscale),
		G_SIGNAL_MATCH_FUNC, 0, 0, 0, G_CALLBACK (cb_fov_changed), 0);
}