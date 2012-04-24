/*
 * main.c
 *
 * Copyright 2011-2012 Thomas L Dunnick
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdio.h>
#include <signal.h>
#include "util.h"
#include "log.h"
#include "xml.h"
#include "task.h"
#include "queue.h"
#include "fpoller.h"
#include "qpoller.h"
#include "ebxml.h"

#ifndef VERSION
#define VERSION "0.5 04/24/2012"
#endif

#ifdef __SENDER__
#ifdef __RECEIVER__
char Software[] = "PHINEAS " VERSION;
#else /* just SENDER */
char Software[] = "PHINEAS Sender " VERSION;
#endif /* RECEIVER and SENDER */
#elif __RECEIVER__
char Software[] = "PHINEAS Receiver " VERSION;
#else /* WTF? */
char Software[] = "PHINEAS " VERSION;
#endif /* __SENDER__ */

#define SLEEP_TIME 1000

#ifndef CMDLINE
char ClassName[20];
#define THIS_CLASSNAME ClassName
#define THIS_TITLE Software
#endif

#define PHINEAS_RUNNING 0
#define PHINEAS_START 1
#define PHINEAS_RESTART 2
#define PHINEAS_SHUTDOWN 3
#define PHINEAS_STOPPED 4
int Status = PHINEAS_STOPPED;

char LogName[MAX_PATH];
char ConfigName[MAX_PATH];
XML *Config;
TASKQ *Taskq = NULL;

/*
 * return true if server is running or starting
 */
int phineas_running ()
{
  switch (Status)
  {
    case PHINEAS_RUNNING :
    case PHINEAS_START :
      return (1);
    default :
      return (0);
  }
}

/*
 * indicate server should restart
 */
int phineas_restart ()
{
  if (Status != PHINEAS_RUNNING)
    return (-1);
  Status = PHINEAS_RESTART;
  return (0);
}

/*
 * when startup fails, clean up and issue a failure message
 */
int phineas_fatal (char *fmt, ...)
{
  va_list ap;
  char buf[2048];

#ifdef SERVICE
  return (-1);
#endif

  strcpy (buf, "FATAL ERROR: ");
  va_start (ap, fmt);
  vsprintf (buf + 13, fmt, ap); 
  va_end (ap);
  phineas_stop ();
#ifdef CMDLINE
    fputs (buf, stderr);
#else
 MessageBox (NULL, buf, Software, MB_OK);
 SendMessage (FindWindow (THIS_CLASSNAME, THIS_TITLE), WM_CLOSE, 0, 0 );
#endif
  return (-1);
}

/*
 * start the services
 */
int phineas_start (int argc, char **argv)
{
  int i;
  char *ch;
  XMLNODE *n;
  int fileq_connect (QUEUECONN *conn);
  extern int server_task (void *p);

  Status = PHINEAS_START;
  loadpath (argv[0]);
  *ConfigName = 0;
  for (i = 1; i < argc; i++)
  {
    if (argv[i][0] != '-')
    {
      strcpy (ConfigName, argv[i]);
    }
  }
  if (*ConfigName == 0)
  {
    pathf (ConfigName, "Phineas.xml");
  }
  if ((Config = xml_load (ConfigName)) == NULL)
  {
    return (phineas_fatal ("Can't load configuration file %s", 
	ConfigName));
  }
  /*
   * make sure our OPENSSL dll's are available...
   */
  if (GetModuleHandle ("ssleay32") == 0)
    return (phineas_fatal ("ssleay32.dll not found"));
  if (GetModuleHandle ("libeay32") == 0)
    return (phineas_fatal ("libeay.dll not found"));
  /*
   * set up the load path and logging
   */
  loadpath (xml_get_text (Config, "Phineas.InstallDirectory"));
  if (pathf (LogName, xml_get_text (Config, "Phineas.LogFile")) == NULL) 
    pathf (LogName,  "Phineas.log");
#ifdef DEBUG
  unlink (LogName);
#else
  backup (LogName);
#endif
  if ((LOGFILE = log_open (LogName)) == NULL)
    return (phineas_fatal ("Unable to open log file %s\n", LogName));
  log_level (LOGFILE, xml_get_text (Config, "Phineas.LogLevel"));

  info ("%s is starting\n", Software);
  debug ("%d args - initializing network\n", argc);
  net_startup ();
  debug ("loading queue configuration\n");
  debug ("initializing queues\n");
  if (queue_init (Config))
    return (phineas_fatal ("Can't initialize queues\n"));
  Taskq = task_allocq (3, 1000);
#ifdef __SERVER__
  debug ("initializing server\n");
  task_add (Taskq, server_task, Config);
  sleep (1);
#endif
#ifdef __SENDER__
  debug ("initializing sender\n");
  fpoller_register ("ebxml", ebxml_fprocessor);
  task_add (Taskq, fpoller_task, Config);
  sleep (1);
  qpoller_register ("EbXmlSndQ", ebxml_qprocessor);
  task_add (Taskq, qpoller_task, Config);
  sleep (1);
#endif 
  Status = PHINEAS_RUNNING;
  info ("Initialzation complete - %s is running\n", Software);
  return (0);
}

/*
 * shutdown the server
 */
int phineas_stop ()
{
  Status = PHINEAS_SHUTDOWN;
  debug ("Stopping phineas\n");
  if (Taskq != NULL)
  {
    debug ("shutting down tasks...\n");
    task_stop (Taskq);
    Taskq = task_freeq (Taskq);
  }
  debug ("shutting down queuing...\n");
  queue_shutdown ();
  debug ("freeing up xml...\n");
  if (Config != NULL)
    Config = xml_free (Config);
  debug ("shutting down networking...\n");
  net_shutdown ();
  debug ("resetting configuration...\n");
  config_reset ();
  info ("%s is stopped\n", Software); 
  LOGFILE = log_close (LOGFILE);
  Status = PHINEAS_STOPPED;
  return (0);
}

/****************** windows service code *******************************/

#ifdef SERVICE

SERVICE_STATUS ServiceStatus; 
SERVICE_STATUS_HANDLE hStatus; 

#define START_TIME (SLEEP_TIME*4)
// #define __SERVICELOG__
#ifdef __SERVICELOG__
#define LOGNAME "C:\\usr\\src\\phineas\\logs\\service.log"
#define WriteToLog(fmt...) \
  Write2Log("%s %d-",__FILE__,__LINE__),Write2Log(fmt)
int Write2Log (char* str, ...)
{
  va_list ap;
  FILE* log;

  log = fopen (LOGNAME, "a+");
  if (log == NULL)
    return -1;
  va_start (ap, str);
  vfprintf(log, str, ap);
  fclose(log);
  va_end (ap);
  return 0;
}
#else
#define WriteToLog(fmt...)
#endif

/*
 * initialize the service
 */
int main_init (int argc, char **argv)
{
  SC_HANDLE SCManager, MyService;
  LPQUERY_SERVICE_CONFIG config;
  DWORD szneeded = 0;
  char *prog;
  int code;
  char buf[1024];

  /*
   * get the full path to our service through the manager
   */
  if ((SCManager = OpenSCManager (NULL, NULL, 0)) == NULL)
  {
    WriteToLog ("failed to open SCM\n");
    return (-1);
  }
  if ((MyService = OpenService (SCManager, argv[0], SERVICE_QUERY_CONFIG))
   == NULL)
  {
    CloseServiceHandle (SCManager);
    WriteToLog ("failed to open handle to %s\n", argv[0]);
    return (-1);
  }
  config = (LPQUERY_SERVICE_CONFIG) buf;
  if (!QueryServiceConfig (MyService, config, sizeof (buf), &szneeded))
  {
    CloseServiceHandle (MyService);
    CloseServiceHandle (SCManager);
    WriteToLog ("failed to get config for %s sz=%d needed=%d\n",
      Software, sizeof (buf), szneeded);
    return (-1);
  }
  CloseServiceHandle (MyService);
  CloseServiceHandle (SCManager);


  WriteToLog ("starting server for %s %s\n", config->lpBinaryPathName,
      argc > 1 ? argv[1] : "");
  prog = argv[0];
  argv[0] = config->lpBinaryPathName;
  if (code = phineas_start (argc, argv))
    WriteToLog ("Startup failed!\n");
  else
    WriteToLog ("initialization complete\n");
  argv[0] = prog;
  return (code);
}

void main_controller (DWORD request) 
{
  switch (request) 
  { 
    case SERVICE_CONTROL_STOP: 
    case SERVICE_CONTROL_SHUTDOWN: 
      WriteToLog ("Stop service request...\n");
      ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
      SetServiceStatus (hStatus, &ServiceStatus);
      break;
    default:
      break;
  } 
  // Report current status
  return; 
}

void main_service (int argc, char** argv) 
{
  ServiceStatus.dwServiceType = SERVICE_WIN32; 
  ServiceStatus.dwCurrentState = SERVICE_START_PENDING; 
  ServiceStatus.dwControlsAccepted =  
   SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
  ServiceStatus.dwWin32ExitCode = 0; 
  ServiceStatus.dwServiceSpecificExitCode = 0; 
  ServiceStatus.dwCheckPoint = 0; 
  ServiceStatus.dwWaitHint = START_TIME; 
  
  WriteToLog ("Registering service\n");
  hStatus = RegisterServiceCtrlHandler(Software,
   (LPHANDLER_FUNCTION) main_controller); 
  if (hStatus == (SERVICE_STATUS_HANDLE) 0) 
  { 
   // Registering Control Handler failed
   return; 
  }  
  // Initialize Service 
  WriteToLog ("Initializing\n");
  if (main_init (argc, argv)) 
  {
   // Initialization failed
   ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
   ServiceStatus.dwWin32ExitCode = -1; 
   SetServiceStatus (hStatus, &ServiceStatus); 
   WriteToLog ("Initializing failed!\n");
   return; 
  } 
  // We report the running status to SCM. 
  ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
  SetServiceStatus (hStatus, &ServiceStatus);
  
  // The worker loop of a service
  WriteToLog ("Worker loop\n");
  while (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
  {
    switch (Status)
    {
      case PHINEAS_RUNNING : 
        sleep (SLEEP_TIME);
	break;
      case PHINEAS_SHUTDOWN : 
	debug ("SHUTDOWN\n");
      case PHINEAS_STOPPED :
	debug ("STOPPED\n");
        ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
	break;
      case PHINEAS_RESTART :
	debug ("RESTARTING\n");
	if (phineas_stop ())
	{
	  WriteToLog ("stop failed!\n");
          ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
	}
        WriteToLog ("attempting to re-initialize...\n");
	if (main_init (argc, argv))
	{
	  WriteToLog ("start failed!\n");
          ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
	}
	break;
    }
  }
  phineas_stop ();
  WriteToLog ("Existing service\n");
  ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
  ServiceStatus.dwWin32ExitCode = 0; 
  SetServiceStatus (hStatus, &ServiceStatus);
  return; 
}

void main ()
{
  SERVICE_TABLE_ENTRY ServiceTable[2];

#ifdef LOGNAME
  unlink (LOGNAME);
#endif

  WriteToLog ("Dispatching service\n");
  ServiceTable[0].lpServiceName = Software;
  ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION) main_service;
  ServiceTable[1].lpServiceName = NULL;
  ServiceTable[1].lpServiceProc = NULL;
  // Start the control dispatcher thread for our service
  StartServiceCtrlDispatcher (ServiceTable);  
  WriteToLog ("Exiting main\n");
}

#endif /* SERVICE */

#ifdef CMDLINE

/*
 * signal handler for exiting the server
 */

void catch_sig (int sig)
{
  char buf[80];
  printf ("Restart server? ");
  if ((fgets (buf, 80, stdin) != NULL) && (toupper (*buf) == 'Y'))
    Status = PHINEAS_RESTART;
  else
    Status = PHINEAS_SHUTDOWN;
}

main (int argc, char **argv)
{
  signal (SIGINT, catch_sig);
  phineas_start (argc, argv);
  while (Status != PHINEAS_STOPPED)
  {
    switch (Status)
    {
      case PHINEAS_RUNNING : 
        sleep (SLEEP_TIME);
	break;
      case PHINEAS_SHUTDOWN : 
	debug ("SHUTDOWN\n");
	phineas_stop ();
      case PHINEAS_STOPPED :
	debug ("STOPPED\n");
	break;
      case PHINEAS_RESTART :
	debug ("RESTARTING\n");
	if (!(phineas_stop () || phineas_start (argc, argv)))
          signal (SIGINT, catch_sig);
	break;
    }
  }
  exit (0);
}

#else /* the GUI version... */

#ifndef SERVICE

#include <winsvc.h>
#include <shellapi.h>

#ifdef __SENDER__
#define PHINEAS_ICON_ID 102
#else
#define PHINEAS_ICON_ID 103
#endif

/* notes whether we have a popup running */
static BOOL InPopup       = FALSE;

/* Icon tray items */
enum 
{
  ID_TRAYICON         = 1,
  APPWM_TRAYICON      = WM_APP,
  APPWM_NOP           = WM_APP + 1,
  /* our own commands */
  ID_STATUS = 2000,
  ID_CONSOLE,
  ID_LOG,
  ID_CONFIG,
  ID_STOP,
  ID_START,
  ID_EXIT
};


/* prototypes... */
void    w_addTrayIcon (HWND hWnd, UINT uID, UINT uCallbackMsg, UINT uIcon,
LPSTR pszToolTip);
void    w_remTrayIcon (HWND hWnd, UINT uID);
HICON	w_loadFileIcon (char *n, int h, int w);
HICON	w_loadResourceIcon (int id, int h, int w);
BOOL    w_popup (HWND hWnd, POINT *curpos, int wDefaultItem);
void    w_init_popup (HWND hWnd, HMENU hMenu, UINT uID);
BOOL    w_command (HWND hWnd, WORD wID, HWND hCtl);
void    w_rightclick (HWND hWnd);
void    w_dblclick (HWND hWnd);
void    w_close (HWND hWnd);
void    w_register (HINSTANCE hInstance);
void    w_unregister (HINSTANCE hInstance);

/* this instance... */
static HINSTANCE myInstance = NULL;
struct mg_context *ctx;


int WINAPI
WinMain (HINSTANCE hInst, HINSTANCE prev, LPSTR cmdline, int show)
{
  int done;
  HMENU   hSysMenu    = NULL;
  HWND    hWnd        = NULL;
  MSG     msg;

  /* set up a unique classname based on our PID */
  sprintf (ClassName, "PHINEAS_%d", getpid ());

  /* let windows know we are here... */
  myInstance = hInst;
  w_register (hInst);

  /*
   * We have to have a window, even though we never show it.  This is
   * because the tray icon uses window messages to send notifications to
   * its owner.  Starting with Windows 2000, you can make some kind of
   * "message target" window that just has a message queue and nothing
   * much else, but we'll be backwardly compatible here.
   */

  hWnd = CreateWindow (THIS_CLASSNAME, THIS_TITLE,
    0, 0, 0, 100, 100, NULL, NULL, hInst, NULL);

  if (!hWnd)
  {
    MessageBox (NULL, "Unable to create main window!", THIS_TITLE,
      MB_ICONERROR | MB_OK | MB_TOPMOST);
    return 1;
  }

  /* initialize the server and start it up */
  done = w_start ();
  debug ("entering service loop\n");
  while (!done)
  {
    /* loop getting and dispatching messages */
    while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
    {
      debug ("Peek message\n");
      if (GetMessage (&msg, NULL, 0, 0)) 
      {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
      }
      else
      {
        debug ("QUIT message\n");
	done = 1;
      }
    }
    debug ("checking status %d\n", Status);
    switch (Status)
    {
      case PHINEAS_RUNNING : 
        sleep (SLEEP_TIME);
	break;
      case PHINEAS_SHUTDOWN : 
	debug ("SHUTDOWN\n");
	phineas_stop ();
      case PHINEAS_STOPPED :
	debug ("STOPPED\n");
	break;
      case PHINEAS_RESTART :
	debug ("RESTARTING\n");
        if (phineas_stop () || w_start ())
	  done = 1;
	break;
    }
  }
  /* clean up and die */
  if (Status != PHINEAS_STOPPED)
  {
    phineas_stop ();
  }
  w_unregister (hInst);
  return msg.wParam;
}

char *w_getarg (char *buf, char *cmd)
{
  char *ch;
  int stop;

  if (*cmd == '"') stop = *cmd++;
  else stop = ' ';
  ch = buf;
  while (*cmd && (*cmd != stop))
    *ch++ = *cmd++;
  *ch = 0;
  if (*cmd == '"') cmd++;
  while (isspace (*cmd)) cmd++;
  return (cmd);
}

#define MAX_ARGS 10

int w_start ()
{
  int e, argc;
  char *argv[MAX_ARGS];
  char buf[MAX_PATH];
  DBUF *b;
  char *cmd;
  
  if ((cmd = GetCommandLine ()) == NULL)
    return (-1);
  b = dbuf_alloc ();
  for (argc = 0; argc < MAX_ARGS; argc++)
  {
    cmd = w_getarg (buf, cmd);
    if (*buf == 0)
      break;
    argv[argc] = dbuf_getbuf (b);
    dbuf_write (b, buf, strlen (buf) + 1);
  }
  e = phineas_start (argc, argv);
  dbuf_free (b);
  return (e);
}

/************************** Tray Related ***************************/

//  Add an icon to the system tray.
void w_addTrayIcon (HWND hWnd, UINT uID, UINT uCallbackMsg, UINT uIcon,
  LPSTR pszToolTip)
{
  NOTIFYICONDATA  nid;

  memset (&nid, 0, sizeof (nid));

  nid.cbSize              = sizeof (nid);
  nid.hWnd                = hWnd;
  nid.uID                 = uID;
  nid.uFlags              = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage    = uCallbackMsg;
  nid.hIcon = w_loadResourceIcon (PHINEAS_ICON_ID, 32, 32);
  strcpy (nid.szTip, pszToolTip);
  Shell_NotifyIcon (NIM_ADD, &nid);
}


//  Remove an icon from the system tray. - called on close
void w_remTrayIcon (HWND hWnd, UINT uID)
{
  NOTIFYICONDATA  nid;

  memset (&nid, 0, sizeof (nid));
  nid.cbSize  = sizeof (nid);
  nid.hWnd    = hWnd;
  nid.uID     = uID;
  Shell_NotifyIcon (NIM_DELETE, &nid);
}


//  OUR NERVE CENTER.
static LRESULT CALLBACK 
w_message (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg) 
  {
    case WM_CREATE:
      w_addTrayIcon (hWnd, ID_TRAYICON, APPWM_TRAYICON, 0, THIS_TITLE);
      return 0;

    case APPWM_NOP:
      //  needed to properly close the tray popup
      return 0;

      //  This is the message which brings tidings of mouse events involving
      //  our tray icon.  We defined it ourselves.  See w_addTrayIcon() for
      //  details of how we told Windows about it.
    case APPWM_TRAYICON:
      SetForegroundWindow (hWnd);

      switch (lParam) 
      {
        case WM_RBUTTONUP:
          w_rightclick (hWnd);
          return 0;

        case WM_LBUTTONDBLCLK:
          w_dblclick (hWnd);
          return 0;
      }
      return 0;

    case WM_COMMAND:
      return w_command (hWnd, LOWORD (wParam), (HWND)lParam);

    case WM_INITMENUPOPUP:
      w_init_popup (hWnd, (HMENU)wParam, lParam);
      return 0;

    case WM_CLOSE:
      w_close (hWnd);
      return DefWindowProc (hWnd, uMsg, wParam, lParam);

    default:
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }
}


// we gone by by
void w_close (HWND hWnd)
{
  //  Remove icon from system tray.
  debug ("Closing service tray and posting QUIT message\n");
  w_remTrayIcon (hWnd, ID_TRAYICON);
  PostQuitMessage (0);
}


//  Create and display our little popupmenu when the user right-clicks 
//  on the system tray icon.
#define MPOS (MF_BYPOSITION|MF_STRING)
#define TMPOPT (TPM_LEFTALIGN|TPM_RIGHTBUTTON|TPM_RETURNCMD|TPM_NONOTIFY)
BOOL w_popup (HWND hWnd, POINT *curpos, int wDefaultItem)
{
  HMENU   hPop        = NULL;
  int     i           = 0;
  WORD    cmd;
  POINT   pt;

  if (InPopup)
    return FALSE;

  hPop = CreatePopupMenu ();

  if (! curpos) 
  {
    GetCursorPos (&pt);
    curpos = &pt;
  }

  InsertMenu (hPop, i++, MPOS, ID_STATUS, "Status");
  InsertMenu (hPop, i++, MPOS, ID_CONSOLE, "Console");
  InsertMenu (hPop, i++, MPOS, ID_LOG, "Log");
  InsertMenu (hPop, i++, MPOS, ID_CONFIG, "Configuration");
  InsertMenu (hPop, i++, MPOS, ID_STOP, "Stop");
  InsertMenu (hPop, i++, MPOS, ID_START, "(re) Start");
  InsertMenu (hPop, i++, MPOS, ID_EXIT, "Shut Down");

  SetMenuDefaultItem (hPop, ID_EXIT, FALSE);
  SetFocus (hWnd);
  SendMessage (hWnd, WM_INITMENUPOPUP, (WPARAM) hPop, 0);
  cmd = TrackPopupMenu (hPop, TMPOPT, curpos->x, curpos->y, 0, hWnd, NULL);
  SendMessage (hWnd, WM_COMMAND, cmd, 0);
  DestroyMenu (hPop);
  return cmd;
}

/* popup the log file */
w_logMessage ()
{
  if ((int) ShellExecute (NULL, "open", LogName, NULL,
    NULL, SW_SHOWNORMAL) < 32)
  {
    MessageBox (NULL, "Failed getting log!", Software, MB_OK);
  }
}

/* popup a status message */
w_statusMessage ()
{
  char *ch;
  DBUF *b = dbuf_alloc ();
  /*
    if (ShellExecute (NULL, "open", "http://www.microsoft.com", 
    NULL, NULL, SW_SHOWNORMAL) < 32)
    {
      MessageBox (NULL, "Failed getting status!", Software, MB_OK);
    }
   */

  // TODO build status message
  switch (Status)
  {
    case PHINEAS_RUNNING : ch = "running"; break;
    case PHINEAS_SHUTDOWN : ch = "shutting down"; break;
    case PHINEAS_STOPPED : ch = "stopped"; break;
    default : ch = "???"; break;
  }
  dbuf_printf (b, "%s is %s\n", THIS_TITLE, ch);
  if (Taskq != NULL)
    dbuf_printf (b, "  %d running threads\n  %d waiting threads\n",
      task_running (Taskq), task_waiting (Taskq));
  MessageBox (NULL, dbuf_getbuf (b), Software, MB_OK);
  dbuf_free (b);
}

/* popup the current configuration */
w_openMessage (char *name)
{
  char *ch;
  int i, j;
  DBUF *b;

  if ((name == NULL) || ((int) ShellExecute (NULL, "open", name,
    NULL, NULL, SW_SHOWNORMAL) < 32))
  { 
    b = dbuf_alloc ();
    if (name == NULL)
      name = "UNKNOWN";
    dbuf_printf (b, "Server Configuration\n\n%s\n", name);
    MessageBox (NULL, dbuf_getbuf (b), Software, MB_OK);
    dbuf_free (b);
  }
}

/* run the console */
w_console ()
{
  char *ch, *proto, *port;
  char path[MAX_PATH];

  port = xml_get_text (Config, "Phineas.Server.SSL.Port");
  proto = "http";
  if (*port)
    proto = "https";
  else
    port = xml_get_text (Config, "Phineas.Server.Port");

  sprintf (path, "%s://localhost:%s%s/console.html",
    proto, port, xml_get_text (Config, "Phineas.Console.Url"));
  w_openMessage (path);
}

// handles command from the icon pop-up
BOOL w_command (HWND hWnd, WORD wID, HWND hCtl)
{
  if (InPopup)
    return 1;

  //  Have a look at the command and act accordingly
  switch (wID) 
  {
    case ID_CONFIG:
      w_openMessage (ConfigName);
      break;

    case ID_STATUS:
      w_statusMessage ();
      break;

    case ID_CONSOLE:
      w_console ();
      break;

    case ID_LOG:
      w_logMessage ();
      break;

    case ID_STOP:
      if (Status == PHINEAS_RUNNING)
        phineas_stop ();
      break;

    case ID_START:
    {
      int e = 0;
      switch (Status)
      {
	case  PHINEAS_RUNNING :
	  Status = PHINEAS_RESTART;
	  break;
	case PHINEAS_STOPPED :
	  e = w_start ();
	  break;
      }
      if (e == 0)
        break;
    }

    case ID_EXIT:
      phineas_stop ();
      debug ("posting close message\n");
      PostMessage (hWnd, WM_CLOSE, 0, 0);
      break;
  }
  return (0);
}

/* 
 * Tray icon is right clicked, bring up our tray menu popup.
 * Even though our main window is not visible, we need to
 * SetForegroundWindow with it, and subsequently post the NOP
 * message to it to get the popup to disappear when we finish with it.
 */
void w_rightclick (HWND hWnd)
{
  SetForegroundWindow (hWnd);
  w_popup (hWnd, NULL, -1);
  PostMessage (hWnd, APPWM_NOP, 0, 0);
}


// double clicks on our tray icon
void w_dblclick (HWND hWnd)
{
   w_statusMessage ();
}

// initialize our tray popup
void w_init_popup (HWND hWnd, HMENU hPop, UINT uID)
{
  // nothing needs to be done
}

// register the main window, which isn't actually visible (:-)
void w_register (HINSTANCE hInstance)
{
  WNDCLASSEX wclx;
  memset (&wclx, 0, sizeof (wclx));

  wclx.cbSize = sizeof (wclx);
  wclx.style = 0;
  wclx.lpfnWndProc = &w_message;
  wclx.cbClsExtra = 0;
  wclx.cbWndExtra = 0;
  wclx.hInstance = hInstance;
  wclx.hCursor = LoadCursor (NULL, IDC_ARROW);
  wclx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);  
  wclx.lpszMenuName = NULL;
  wclx.lpszClassName = THIS_CLASSNAME;

  RegisterClassEx (&wclx);
}

// remove main window before expiring
void w_unregister (HINSTANCE hInstance)
{
  UnregisterClass (THIS_CLASSNAME, hInstance);
}

// load an icon from a resource
HICON w_loadResourceIcon (int id, int h, int w)
{
  return LoadImage (myInstance, MAKEINTRESOURCE(id), IMAGE_ICON, h, w, 0);
}

// load an icon from a file
HICON w_loadFileIcon (char *fname, int h, int w)
{
  return LoadImage (NULL, fname, IMAGE_ICON, h, w, LR_LOADFROMFILE);
}
#endif /* !SERVICE */
#endif /* !CMDLINE */
