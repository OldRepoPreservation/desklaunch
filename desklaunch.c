/* 
 * desklaunch
 *
 * Copyright (C) 2001 Ken Lynch
 * Copyright (C) 2002 Stefan Pfetzing
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define TOOLTIP_COL		"#fffacd"
#define TOOLTIP_FONT		"lucidasans-12"

#define WIN_STATE		0
#define WIN_HINTS		1
#define WIN_LAYER		2
#define GNOME_HINT_COUNT	3

#define WIN_STATE_STICKY	(1<<0)
#define WIN_HINTS_SKIP_FOCUS	(1<<0)
#define WIN_HINTS_SKIP_WINLIST	(1<<1)
#define WIN_HINTS_SKIP_TASKBAR	(1<<2)

#define MWM_DECORATIONS		(1<<1)

typedef struct
{
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  unsigned long inputMode;
  unsigned long status;
} PropMwmHints;

typedef struct _Icon Icon;

struct _Icon
{
  Pixmap pixmap, mask;
  int x, y, width, height;
  Window window;
  char *pixmap_file, *tooltip, *command;
  Icon *next;
};

struct
{
  Window window;
  XFontStruct *font;
  GC gc;
} tooltip;

Display *dpy;
int screen;
Window root;
Icon *icon=NULL, *active_icon=NULL;
time_t last_update;
Atom gnome[GNOME_HINT_COUNT], motif;

/*
 *
 * General functions
 *
 */
 
void print_error(char *error, int critical)
{
#ifdef DEBUG
  printf("print_error\n");
#endif

  fprintf(stderr, "DESKLAUNCH: %s", error);
  if(critical) exit(1);
}

void set_gnome_hint(Window w, int a, long value)
{
#ifdef DEBUG
  printf("set_gnome_hint\n");
#endif

  XChangeProperty(dpy, w, gnome[a], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&value, 1);
}

void set_mwm_hints(Window w, PropMwmHints *hints)
{
#ifdef DEBUG
  printf("set_mwm_hints");
#endif

  XChangeProperty(dpy, w, motif, motif, 32, PropModeReplace, (unsigned char *)hints, sizeof(*hints));
}

time_t get_last_update(char *dir, char *file)
{
  char *file_name;
  struct stat stat_buf;

  if((file_name=malloc(strlen(dir)+strlen(file)+2))==NULL)
    print_error("Memory allocation failed.\n", True);

  sprintf(file_name, "%s/%s", dir, file);

  /* If no rc file then set time equal to last_update */
  if(stat(file_name, &stat_buf))
    stat_buf.st_mtime=last_update;
  free(file_name);
  return stat_buf.st_mtime;
}

/*
 *
 * Icon functions
 *
 */
void create_icon(char *icon_string)
{
  XSetWindowAttributes attr;
  XpmAttributes xpm_attr;
  Icon *i;
  Pixmap pixmap, mask;
  int x=0, y=0;
  char *rvalue=NULL, *pixmap_file=NULL, *text=NULL, *command=NULL;
  XSizeHints size_hints;
  PropMwmHints mwm_hints;

#ifdef DEBUG
  printf("create_icon\n");
#endif

  rvalue=strtok(icon_string, ":");
  if(rvalue)
  {           
    x=atoi(rvalue);
    rvalue=strtok(NULL, ":");
    if(rvalue)
    {
      y=atoi(rvalue);
      pixmap_file=strtok(NULL, ":");
      if(pixmap_file)
      {
        text=strtok(NULL, ":");
        if(text)
          command=strtok(NULL, "\n");
      }
    }
  }

  if(pixmap_file==NULL || text==NULL || command==NULL) return;

  xpm_attr.valuemask=XpmSize;
  if(XpmReadFileToPixmap(dpy, root, pixmap_file, &pixmap, &mask, &xpm_attr)==0)
  {
    if((i=malloc(sizeof *i))==NULL)
      print_error("Failed to allocate memory for icon.", True);
    i->x=x;
    i->y=y;
    i->width=xpm_attr.width;
    i->height=xpm_attr.height;
    i->pixmap_file=strdup(pixmap_file);
    i->tooltip=strdup(text);
    i->command=strdup(command);
    i->next=icon;
    i->pixmap=pixmap;
    i->mask=mask;
    icon=i;

    attr.event_mask=ButtonPressMask|ButtonReleaseMask|EnterWindowMask|LeaveWindowMask|StructureNotifyMask;
    attr.override_redirect=False;
    attr.background_pixel=0;
    i->window=XCreateWindow(dpy, root, i->x, i->y, i->width, i->height, 0, DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen), CWOverrideRedirect|CWBackPixel|CWEventMask, &attr);
    XSetWindowBackgroundPixmap(dpy, i->window, i->pixmap);
    XShapeCombineMask(dpy, icon->window, ShapeBounding, 0, 0, i->mask, ShapeSet);

    set_gnome_hint(i->window, WIN_STATE, WIN_STATE_STICKY);
    set_gnome_hint(i->window, WIN_HINTS, WIN_HINTS_SKIP_FOCUS|WIN_HINTS_SKIP_TASKBAR|WIN_HINTS_SKIP_WINLIST);
    set_gnome_hint(i->window, WIN_LAYER, 0);

    mwm_hints.flags=MWM_DECORATIONS;
    mwm_hints.decorations=0;
    set_mwm_hints(i->window, &mwm_hints);

    size_hints.flags=PPosition;
    XSetWMNormalHints(dpy, i->window, &size_hints);

    XMapWindow(dpy, i->window);
    XRaiseWindow(dpy, i->window);
  }
  else
    print_error("Xpm file not found.", False);
}

Icon *find_icon_from_window(Window w)
{
  Icon *i;

#ifdef DEBUG
  printf("find_icon_from_window\n");
#endif

  for(i=icon; i!=NULL; i=i->next)
    if(i->window==w) return i;
  return NULL;
}

void free_icons()
{
  Icon *i;

#ifdef DEBUG
  printf("free_icons\n");
#endif

  while(icon)
  {
    XDestroyWindow(dpy, icon->window);
    XFreePixmap(dpy, icon->pixmap);
    XFreePixmap(dpy, icon->mask);
    free(icon->pixmap_file);
    free(icon->command);
    i=icon->next;
    free(icon);
    icon=i;
  }
}

/*
 *
 * Rc file functions
 *
 */

void parse_rc(char *dir, char *file)
{
  FILE *rc;
  char *rc_file, buf[1024], *lvalue, *rvalue;

#ifdef DEBUG
  printf("parse_rc\n");
#endif

  if((rc_file=malloc(strlen(dir)+strlen(file)+2))==NULL)
    return;
 
  sprintf(rc_file, "%s/%s", dir, file); 

  if((rc=fopen(rc_file, "r")))
  {
    while(fgets(buf, sizeof buf, rc))
    {
      lvalue=strtok(buf, "=");
      if(lvalue)
      {
        if(!strcmp(lvalue, "icon"))
        {
          rvalue=strtok(NULL, "\n");
          if(rvalue)
            create_icon(rvalue);
        }
      }
    }
  }
  last_update=get_last_update(getenv("HOME"), RCFILE);
}

void save_icons(char *dir, char *file)
{
  char *rc_file;
  Icon *i;
  FILE *rc;

#ifdef DEBUG
  printf("save_icons\n");
#endif

  if((rc_file=malloc(strlen(dir)+strlen(file)+2))==NULL)
    return;
 
  sprintf(rc_file, "%s/%s", dir, file); 

  if((rc=fopen(rc_file, "w")))
  {
    fprintf(rc, "#\n# .desklauchrc created automaticlly by desklaunch\n#\n");
    for(i=icon;i;i=i->next)
      fprintf(rc, "icon=%d:%d:%s:%s:%s\n", i->x, i->y, i->pixmap_file, i->tooltip, i->command);
    fclose(rc);
  }
  last_update=get_last_update(getenv("HOME"), RCFILE);
}

/*
 *
 * Execute command
 *
 */
 
void fork_exec(char *cmd)
{
  pid_t pid=fork();

#ifdef DEBUG
  printf("fork_exec\n");
  printf("  executing %s\n", cmd);
#endif

  switch(pid)
  {
  case 0:
    execlp("/bin/sh", "sh", "-c", cmd, NULL);
    fprintf(stderr, "Exec failed.\n");
    exit(0);
    break;
  case -1:
    fprintf(stderr, "Fork failed.\n");
    break;
  }
}

/*
 *
 * Event handlers
 *
 */
 
void event_enter_notify(XCrossingEvent *ev)
{
  Icon *i;
  int x, width, height;

#ifdef DEBUG
  printf("event_enter_notify\n");
#endif

  i=find_icon_from_window(ev->window);

  if(i)
  {
    width=XTextWidth(tooltip.font, i->tooltip, strlen(i->tooltip))+2;
    height=tooltip.font->ascent+tooltip.font->descent+2;
    x=i->x+(i->width/2)-(width/2)-1;
    XMoveResizeWindow(dpy, tooltip.window, x, i->y+i->height+4, width, height);
    XMapRaised(dpy, tooltip.window);
    XDrawString(dpy, tooltip.window, tooltip.gc, 1, tooltip.font->ascent+1, i->tooltip, strlen(i->tooltip));
    active_icon=i;
  }
}

void event_leave_notify(XCrossingEvent *ev)
{
#ifdef DEBUG
  printf("event_leave_notify\n");
#endif

  XClearWindow(dpy, tooltip.window);
  XUnmapWindow(dpy, tooltip.window);
  active_icon=NULL;
}

void event_button_release(XButtonEvent *ev)
{
#ifdef DEBUG
  printf("event_button_release\n");
#endif

  if(active_icon)
  {
    if(ev->window==active_icon->window && ev->button==Button1 && ev->state==256)
      fork_exec(active_icon->command);
  }
}

void event_configure_notify(XConfigureEvent *ev)
{
  Icon *i;

#ifdef DEBUG
  printf("event_configure_notify\n");
#endif

  i=find_icon_from_window(ev->window);
  if(i)
  {
    if(i->x!=ev->x || i->y!=ev->y)
    {
      i->x=ev->x;
      i->y=ev->y;
      XUnmapWindow(dpy, tooltip.window);
      save_icons(getenv("HOME"), RCFILE);
    }
  }
}

/*
 *
 * Initialize and quit functions
 *
 */
 
void quit()
{
#ifdef DEBUG
  printf("quit\n");
#endif

  free_icons();
  XDestroyWindow(dpy, tooltip.window);
  XFreeFont(dpy, tooltip.font);
  XFreeGC(dpy, tooltip.gc);
  XCloseDisplay(dpy);
  exit(0);
}

void signal_handler(int signal)
{
#ifdef DEBUG
  printf("signal_handler\n");
#endif

  switch(signal)
  {
  case SIGINT:
  case SIGTERM:
  case SIGHUP:
    quit();
    break;
  case SIGCHLD:
    wait(NULL);
    break;
  }
}

void initialize()
{
  struct sigaction act;
  XSetWindowAttributes attr;
  XColor tooltip_col, dummyc;
  XGCValues gcv;

#ifdef DEBUG
  printf("initialize\n");
#endif

  act.sa_handler=signal_handler;
  act.sa_flags=0;
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGHUP, &act, NULL);
  sigaction(SIGCHLD, &act, NULL);
  
  if(!(dpy=XOpenDisplay(NULL)))
    print_error("Can't open display, X may not be running.\n", True);
    
  screen=XDefaultScreen(dpy);
  root=XDefaultRootWindow(dpy);
  
  XAllocNamedColor(dpy, DefaultColormap(dpy, screen), TOOLTIP_COL, &tooltip_col, &dummyc);
  tooltip.font=XLoadQueryFont(dpy, TOOLTIP_FONT);

  attr.override_redirect=True;
  attr.background_pixel=tooltip_col.pixel;
  tooltip.window=XCreateWindow(dpy, root, 0, 0, 10, 10, 1, DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen), CWOverrideRedirect|CWBackPixel, &attr);

  gcv.function=GXcopy;
  gcv.foreground=0;
  gcv.font=tooltip.font->fid;
  tooltip.gc=XCreateGC(dpy, root, GCFunction|GCForeground|GCFont, &gcv);

  gnome[WIN_STATE]=XInternAtom(dpy, "_WIN_STATE", False);
  gnome[WIN_HINTS]=XInternAtom(dpy, "_WIN_HINTS", False);
  gnome[WIN_LAYER]=XInternAtom(dpy, "_WIN_LAYER", False);
  motif=XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
  
  parse_rc(getenv("HOME"), RCFILE);
}

/*
 *
 * Main
 *
 */

int main(int argc, char *argv[])
{
  initialize();

  /* Event loop */
  while(1)
  {
    XEvent ev;

    while(XPending(dpy))
    {
      XNextEvent(dpy, &ev);
      switch(ev.type)
      {
      case EnterNotify:
        event_enter_notify(&ev.xcrossing);
        break;
      case LeaveNotify:
        event_leave_notify(&ev.xcrossing);
        break;
      case ButtonRelease:
        event_button_release(&ev.xbutton);
        break;
      case ConfigureNotify:
        event_configure_notify(&ev.xconfigure);
        break;
      }
    }

    if(get_last_update(getenv("HOME"), RCFILE)!=last_update)
    {
      free_icons();
      parse_rc(getenv("HOME"), RCFILE);
    }
    usleep(1);
  }
  return 0;
}
