#ifndef __POSIX_SDL_GL_SYSFB_H__
#define __POSIX_SDL_GL_SYSFB_H__

#include <SDL.h>

#include "v_video.h"

class SystemBaseFrameBuffer : public DFrameBuffer
{
	typedef DFrameBuffer Super;

public:
	// this must have the same parameters as the Windows version, even if they are not used!
	SystemBaseFrameBuffer (void *hMonitor, bool fullscreen);

	bool IsFullscreen() override;

	int GetClientWidth() override;
	int GetClientHeight() override;

	void ToggleFullscreen(bool yes) override;
	void SetWindowSize(int client_w, int client_h) override;

protected:
	SystemBaseFrameBuffer () {}
};

class SystemGLFrameBuffer : public SystemBaseFrameBuffer
{
	typedef SystemBaseFrameBuffer Super;

public:
	SystemGLFrameBuffer(void *hMonitor, bool fullscreen);
	~SystemGLFrameBuffer();

	int GetClientWidth() override;
	int GetClientHeight() override;

	virtual void SetVSync(bool vsync) override;
	void SwapBuffers();

	void setNULLContext();
	void setMainContext();
	void setAuxContext(int index);
	int numAuxContexts();

protected:
	SDL_GLContext GLContext;
	SDL_GLContext GLAuxContexts[4] = { NULL, NULL, NULL, NULL };

	SystemGLFrameBuffer() {}
};


#endif // __POSIX_SDL_GL_SYSFB_H__

