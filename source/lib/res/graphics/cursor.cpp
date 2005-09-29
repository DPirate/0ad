#include "precompiled.h"

#include <string.h>
#include <sstream>

// On Windows, allow runtime choice between system cursors and OpenGL
// cursors (Windows = more responsive, OpenGL = more consistent with what
// the game sees)
#define ALLOW_SYS_CURSOR 1

#include "lib/ogl.h"
#include "sysdep/sysdep.h"	// sys_cursor_*
#include "../res.h"
#include "ogl_tex.h"
#include "cursor.h"

// no init is necessary because this is stored in struct Cursor, which
// is 0-initialized by h_mgr.
class GLCursor
{
	Handle ht;
	uint w, h;
	uint hotspotx, hotspoty;

public:
	int create(const char* filename, uint hotspotx_, uint hotspoty_)
	{
		ht = ogl_tex_load(filename);
		RETURN_ERR(ht);

		(void)ogl_tex_get_size(ht, &w, &h, 0);

		hotspotx = hotspotx_; hotspoty = hotspoty_;

		(void)ogl_tex_set_filter(ht, GL_NEAREST);
		(void)ogl_tex_upload(ht);
		return 0;
	}

	void destroy()
	{
		(void)ogl_tex_free(ht);
	}

	void draw(uint x, uint y)
	{
		(void)ogl_tex_bind(ht);
		glEnable(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		// OpenGL's coordinate system is "upside-down"; correct for that.
		y = g_yres - y;

		glBegin(GL_QUADS);
		glTexCoord2i(0, 0); glVertex2i( x-hotspotx,   y+hotspoty   );
		glTexCoord2i(1, 0); glVertex2i( x-hotspotx+w, y+hotspoty   );
		glTexCoord2i(1, 1); glVertex2i( x-hotspotx+w, y+hotspoty-h );
		glTexCoord2i(0, 1); glVertex2i( x-hotspotx,   y+hotspoty-h );
		glEnd();
	}
};


struct Cursor
{
	void* sys_cursor;

	// valid iff sys_cursor == 0.
	GLCursor gl_cursor;
};

H_TYPE_DEFINE(Cursor);

static void Cursor_init(Cursor* UNUSED(c), va_list UNUSED(args))
{
}

static void Cursor_dtor(Cursor* c)
{
	if(c->sys_cursor)
		WARN_ERR(sys_cursor_free(c->sys_cursor));
	else
		c->gl_cursor.destroy();
}

static int Cursor_reload(Cursor* c, const char* name, Handle)
{
	char filename[VFS_MAX_PATH];

	// read pixel offset of the cursor's hotspot [the bit of it that's
	// drawn at (g_mouse_x,g_mouse_y)] from file.
	uint hotspotx = 0, hotspoty = 0;
	{
		snprintf(filename, ARRAY_SIZE(filename), "art/textures/cursors/%s.txt", name);
		void* p; size_t size;
		Handle hm = vfs_load(filename, p, size);
		RETURN_ERR(hm);
		std::stringstream s(std::string((const char*)p, size));
		s >> hotspotx >> hotspoty;
		(void)mem_free_h(hm);
	}

	// load actual cursor
	snprintf(filename, ARRAY_SIZE(filename), "art/textures/cursors/%s.png", name);
	// .. system cursor (2d, hardware accelerated)
#if ALLOW_SYS_CURSOR
	WARN_ERR(sys_cursor_load(filename, hotspotx, hotspoty, &c->sys_cursor));
#endif
	// .. fall back to GLCursor (system cursor code is disabled or failed)
	if(!c->sys_cursor)
		RETURN_ERR(c->gl_cursor.create(filename, hotspotx, hotspoty));

	return 0;
}

// note: these standard resource interface functions are not exposed to the
// caller. all we need here is storage for the sys_cursor / GLCursor and
// a name -> data lookup mechanism, both provided by h_mgr.
// in other words, we continually create/free the cursor resource in
// cursor_draw and trust h_mgr's caching to absorb it.

static Handle cursor_load(const char* name)
{
	return h_alloc(H_Cursor, name, 0);
}

static int cursor_free(Handle& h)
{
	return h_free(h, H_Cursor);
}


// draw the specified cursor at the given pixel coordinates
// (origin is top-left to match the windowing system).
// uses a hardware mouse cursor where available, otherwise a
// portable OpenGL implementation.
int cursor_draw(const char* name, int x, int y)
{
	// Use 'null' to disable the cursor
	if(!name)
	{
		WARN_ERR(sys_cursor_set(0));
		return 0;
	}

	Handle hc = cursor_load(name);
	CHECK_ERR(hc);
	H_DEREF(hc, Cursor, c);

	if(c->sys_cursor)
		WARN_ERR(sys_cursor_set(c->sys_cursor));
	else
		c->gl_cursor.draw(x, y);

	(void)cursor_free(hc);
	return 0;
}
