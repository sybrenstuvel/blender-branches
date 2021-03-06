/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/screen/screen_draw.c
 *  \ingroup edscr
 */

#include "ED_screen.h"

#include "GPU_framebuffer.h"
#include "GPU_immediate.h"

#include "WM_api.h"
#include "WM_types.h"

#include "screen_intern.h"


/**
 * Draw vertical shape visualizing future joining (left as well right direction of future joining).
 */
static void draw_horizontal_join_shape(ScrArea *sa, char dir)
{
	vec2f points[10];
	short i;
	float w, h;
	float width = sa->v3->vec.x - sa->v1->vec.x;
	float height = sa->v3->vec.y - sa->v1->vec.y;

	if (height < width) {
		h = height / 8;
		w = height / 4;
	}
	else {
		h = width / 8;
		w = width / 4;
	}

	points[0].x = sa->v1->vec.x;
	points[0].y = sa->v1->vec.y + height / 2;

	points[1].x = sa->v1->vec.x;
	points[1].y = sa->v1->vec.y;

	points[2].x = sa->v4->vec.x - w;
	points[2].y = sa->v4->vec.y;

	points[3].x = sa->v4->vec.x - w;
	points[3].y = sa->v4->vec.y + height / 2 - 2 * h;

	points[4].x = sa->v4->vec.x - 2 * w;
	points[4].y = sa->v4->vec.y + height / 2;

	points[5].x = sa->v4->vec.x - w;
	points[5].y = sa->v4->vec.y + height / 2 + 2 * h;

	points[6].x = sa->v3->vec.x - w;
	points[6].y = sa->v3->vec.y;

	points[7].x = sa->v2->vec.x;
	points[7].y = sa->v2->vec.y;

	points[8].x = sa->v4->vec.x;
	points[8].y = sa->v4->vec.y + height / 2 - h;

	points[9].x = sa->v4->vec.x;
	points[9].y = sa->v4->vec.y + height / 2 + h;

	if (dir == 'l') {
		/* when direction is left, then we flip direction of arrow */
		float cx = sa->v1->vec.x + width;
		for (i = 0; i < 10; i++) {
			points[i].x -= cx;
			points[i].x = -points[i].x;
			points[i].x += sa->v1->vec.x;
		}
	}

	glBegin(GL_POLYGON);
	for (i = 0; i < 5; i++)
		glVertex2f(points[i].x, points[i].y);
	glEnd();
	glBegin(GL_POLYGON);
	for (i = 4; i < 8; i++)
		glVertex2f(points[i].x, points[i].y);
	glVertex2f(points[0].x, points[0].y);
	glEnd();

	glRectf(points[2].x, points[2].y, points[8].x, points[8].y);
	glRectf(points[6].x, points[6].y, points[9].x, points[9].y);
}

/**
 * Draw vertical shape visualizing future joining (up/down direction).
 */
static void draw_vertical_join_shape(ScrArea *sa, char dir)
{
	vec2f points[10];
	short i;
	float w, h;
	float width = sa->v3->vec.x - sa->v1->vec.x;
	float height = sa->v3->vec.y - sa->v1->vec.y;

	if (height < width) {
		h = height / 4;
		w = height / 8;
	}
	else {
		h = width / 4;
		w = width / 8;
	}

	points[0].x = sa->v1->vec.x + width / 2;
	points[0].y = sa->v3->vec.y;

	points[1].x = sa->v2->vec.x;
	points[1].y = sa->v2->vec.y;

	points[2].x = sa->v1->vec.x;
	points[2].y = sa->v1->vec.y + h;

	points[3].x = sa->v1->vec.x + width / 2 - 2 * w;
	points[3].y = sa->v1->vec.y + h;

	points[4].x = sa->v1->vec.x + width / 2;
	points[4].y = sa->v1->vec.y + 2 * h;

	points[5].x = sa->v1->vec.x + width / 2 + 2 * w;
	points[5].y = sa->v1->vec.y + h;

	points[6].x = sa->v4->vec.x;
	points[6].y = sa->v4->vec.y + h;

	points[7].x = sa->v3->vec.x;
	points[7].y = sa->v3->vec.y;

	points[8].x = sa->v1->vec.x + width / 2 - w;
	points[8].y = sa->v1->vec.y;

	points[9].x = sa->v1->vec.x + width / 2 + w;
	points[9].y = sa->v1->vec.y;

	if (dir == 'u') {
		/* when direction is up, then we flip direction of arrow */
		float cy = sa->v1->vec.y + height;
		for (i = 0; i < 10; i++) {
			points[i].y -= cy;
			points[i].y = -points[i].y;
			points[i].y += sa->v1->vec.y;
		}
	}

	glBegin(GL_POLYGON);
	for (i = 0; i < 5; i++)
		glVertex2f(points[i].x, points[i].y);
	glEnd();
	glBegin(GL_POLYGON);
	for (i = 4; i < 8; i++)
		glVertex2f(points[i].x, points[i].y);
	glVertex2f(points[0].x, points[0].y);
	glEnd();

	glRectf(points[2].x, points[2].y, points[8].x, points[8].y);
	glRectf(points[6].x, points[6].y, points[9].x, points[9].y);
}

/**
 * Draw join shape due to direction of joining.
 */
static void draw_join_shape(ScrArea *sa, char dir)
{
	if (dir == 'u' || dir == 'd')
		draw_vertical_join_shape(sa, dir);
	else
		draw_horizontal_join_shape(sa, dir);
}

/**
 * Draw screen area darker with arrow (visualization of future joining).
 */
static void scrarea_draw_shape_dark(ScrArea *sa, char dir)
{
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4ub(0, 0, 0, 50);
	draw_join_shape(sa, dir);
}

/**
 * Draw screen area ligher with arrow shape ("eraser" of previous dark shape).
 */
static void scrarea_draw_shape_light(ScrArea *sa, char UNUSED(dir))
{
	glBlendFunc(GL_DST_COLOR, GL_SRC_ALPHA);
	/* value 181 was hardly computed: 181~105 */
	glColor4ub(255, 255, 255, 50);
	/* draw_join_shape(sa, dir); */
	glRecti(sa->v1->vec.x, sa->v1->vec.y, sa->v3->vec.x, sa->v3->vec.y);
}

static void drawscredge_area_draw(int sizex, int sizey, short x1, short y1, short x2, short y2)
{
	/* right border area */
	if (x2 < sizex - 1) {
		glVertex2s(x2, y1);
		glVertex2s(x2, y2);
	}

	/* left border area */
	if (x1 > 0) { /* otherwise it draws the emboss of window over */
		glVertex2s(x1, y1);
		glVertex2s(x1, y2);
	}

	/* top border area */
	if (y2 < sizey - 1) {
		glVertex2s(x1, y2);
		glVertex2s(x2, y2);
	}

	/* bottom border area */
	if (y1 > 0) {
		glVertex2s(x1, y1);
		glVertex2s(x2, y1);
	}
}

/**
 * \brief Screen edges drawing.
 */
static void drawscredge_area(ScrArea *sa, int sizex, int sizey)
{
	short x1 = sa->v1->vec.x;
	short y1 = sa->v1->vec.y;
	short x2 = sa->v3->vec.x;
	short y2 = sa->v3->vec.y;

	drawscredge_area_draw(sizex, sizey, x1, y1, x2, y2);
}

/**
 * Only for edge lines between areas, and the blended join arrows.
 */
void ED_screen_draw(wmWindow *win)
{
	const int winsize_x = WM_window_pixels_x(win);
	const int winsize_y = WM_window_pixels_y(win);

	ScrArea *sa;
	ScrArea *sa1 = NULL;
	ScrArea *sa2 = NULL;
	ScrArea *sa3 = NULL;

	wmSubWindowSet(win, win->screen->mainwin);

	/* Note: first loop only draws if U.pixelsize > 1, skip otherwise */
	if (U.pixelsize > 1.0f) {
		/* FIXME: doesn't our glLineWidth already scale by U.pixelsize? */
		glLineWidth((2.0f * U.pixelsize) - 1);
		glColor3ub(0x50, 0x50, 0x50);
		glBegin(GL_LINES);
		for (sa = win->screen->areabase.first; sa; sa = sa->next)
			drawscredge_area(sa, winsize_x, winsize_y);
		glEnd();
	}

	glLineWidth(1);
	glColor3ub(0, 0, 0);
	glBegin(GL_LINES);
	for (sa = win->screen->areabase.first; sa; sa = sa->next) {
		drawscredge_area(sa, winsize_x, winsize_y);

		/* gather area split/join info */
		if (sa->flag & AREA_FLAG_DRAWJOINFROM) sa1 = sa;
		if (sa->flag & AREA_FLAG_DRAWJOINTO) sa2 = sa;
		if (sa->flag & (AREA_FLAG_DRAWSPLIT_H | AREA_FLAG_DRAWSPLIT_V)) sa3 = sa;
	}
	glEnd();

	/* blended join arrow */
	if (sa1 && sa2) {
		int dir = area_getorientation(sa1, sa2);
		int dira = -1;
		if (dir != -1) {
			switch (dir) {
				case 0: /* W */
					dir = 'r';
					dira = 'l';
					break;
				case 1: /* N */
					dir = 'd';
					dira = 'u';
					break;
				case 2: /* E */
					dir = 'l';
					dira = 'r';
					break;
				case 3: /* S */
					dir = 'u';
					dira = 'd';
					break;
			}
		}
		glEnable(GL_BLEND);
		scrarea_draw_shape_dark(sa2, dir);
		scrarea_draw_shape_light(sa1, dira);
		glDisable(GL_BLEND);
	}

	/* splitpoint */
	if (sa3) {
		glEnable(GL_BLEND);
		glBegin(GL_LINES);
		glColor4ub(255, 255, 255, 100);

		if (sa3->flag & AREA_FLAG_DRAWSPLIT_H) {
			glVertex2s(sa3->totrct.xmin, win->eventstate->y);
			glVertex2s(sa3->totrct.xmax, win->eventstate->y);
			glColor4ub(0, 0, 0, 100);
			glVertex2s(sa3->totrct.xmin, win->eventstate->y + 1);
			glVertex2s(sa3->totrct.xmax, win->eventstate->y + 1);
		}
		else {
			glVertex2s(win->eventstate->x, sa3->totrct.ymin);
			glVertex2s(win->eventstate->x, sa3->totrct.ymax);
			glColor4ub(0, 0, 0, 100);
			glVertex2s(win->eventstate->x + 1, sa3->totrct.ymin);
			glVertex2s(win->eventstate->x + 1, sa3->totrct.ymax);
		}
		glEnd();
		glDisable(GL_BLEND);
	}

	win->screen->do_draw = false;
}


/* -------------------------------------------------------------------- */
/* Screen Thumbnail Preview */

/**
 * Calculates a scale factor to squash the preview for \a screen into a rectangle of given size and aspect.
 */
static void screen_preview_scale_get(
        const bScreen *screen, float size_x, float size_y,
        const float asp[2],
        float r_scale[2])
{
	float max_x = 0, max_y = 0;

	for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
		max_x = MAX2(max_x, sa->totrct.xmax);
		max_y = MAX2(max_y, sa->totrct.ymax);
	}
	r_scale[0] = (size_x * asp[0]) / max_x;
	r_scale[1] = (size_y * asp[1]) / max_y;
}

static void screen_preview_draw_areas(const bScreen *screen, const float scale[2], const float col[4],
                                      const float ofs_between_areas)
{
	const float ofs_h = ofs_between_areas * 0.5f;
	unsigned int pos = add_attrib(immVertexFormat(), "pos", GL_FLOAT, 2, KEEP_FLOAT);
	rctf rect;

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor4fv(col);

	for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
		rect.xmin = sa->totrct.xmin * scale[0] + ofs_h;
		rect.xmax = sa->totrct.xmax * scale[0] - ofs_h;
		rect.ymin = sa->totrct.ymin * scale[1] + ofs_h;
		rect.ymax = sa->totrct.ymax * scale[1] - ofs_h;

		immBegin(PRIM_TRIANGLE_FAN, 4);
		immVertex2f(pos, rect.xmin, rect.ymin);
		immVertex2f(pos, rect.xmax, rect.ymin);
		immVertex2f(pos, rect.xmax, rect.ymax);
		immVertex2f(pos, rect.xmin, rect.ymax);
		immEnd();
	}

	immUnbindProgram();
}

static void screen_preview_draw(const bScreen *screen, int size_x, int size_y)
{
	const float asp[2] = {1.0f, 0.8f}; /* square previews look a bit ugly */
	/* could use theme color (tui.wcol_menu_item.text), but then we'd need to regenerate all previews when changing */
	const float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float scale[2];

	wmOrtho2(0.0f, size_x, 0.0f, size_y);
	/* center */
	glTranslatef(size_x * (1.0f - asp[0]) * 0.5f, size_y * (1.0f - asp[1]) * 0.5f, 0.0f);

	screen_preview_scale_get(screen, size_x, size_y, asp, scale);
	screen_preview_draw_areas(screen, scale, col, 1.5f);
}

/**
 * Render the preview for a screen layout in \a screen.
 */
void ED_screen_preview_render(const bScreen *screen, int size_x, int size_y, unsigned int *r_rect)
{
	char err_out[256] = "unknown";
	GPUOffScreen *offscreen = GPU_offscreen_create(size_x, size_y, 0, err_out);

	GPU_offscreen_bind(offscreen, true);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	screen_preview_draw(screen, size_x, size_y);

	GPU_offscreen_read_pixels(offscreen, GL_UNSIGNED_BYTE, r_rect);
	GPU_offscreen_unbind(offscreen, true);

	GPU_offscreen_free(offscreen);
}
