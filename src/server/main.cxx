//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, U$
//
// Copyright (C) 2006  Oliver Schroeder
//

//////////////////////////////////////////////////////////////////////
//
// main program
//
//////////////////////////////////////////////////////////////////////

using namespace std;

#include <cstdlib>
#include <sys/wait.h>
#include <signal.h>
#include "fg_server.hxx"
#include "fg_config.hxx"
#include "daemon.hxx"
#include "typcnvt.hxx"

FG_SERVER       Servant;
extern  bool    RunAsDaemon;
extern  cDaemon Myself;
string          ConfigFileName; // from commandline

//////////////////////////////////////////////////////////////////////
//
//      print a help screen for command line parameters
//
//////////////////////////////////////////////////////////////////////
void
PrintHelp ()
{
  cout << endl;
  cout << "\n"
  "options are:\n"
  "-h            print this help screen\n"
  "-a PORT       listen to PORT for telnet\n"
  "-c config     read 'config' as configuration file\n"
  "-p PORT       listen to PORT\n"
  "-t TTL        Time a client is active while not sending packets\n"
  "-o OOR        nautical miles two players must be apart to be out of reach\n"
  "-l LOGFILE    Log to LOGFILE\n"
  "-v LEVEL      verbosity (loglevel) in range 1 (few) and 5 (much)\n"
  "-d            do _not_ run as a daemon (stay in foreground)\n"
  "-D            do run as a daemon\n"
  "\n"
  "the default is to run as a daemon, which can be overridden in the\n"
  "config file.\n"
  "commandline parameters always override config file options\n"
  "\n";
  exit (0);
} // PrintHelp ()
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
//      read a config file and set internal variables accordingly
//
//////////////////////////////////////////////////////////////////////
bool
ProcessConfig ( const string& ConfigName )
{
  FG_CONFIG   Config;
  string      Val;
  int         E;

  if (Config.Read (ConfigName))
  {
    return (false);
  }
  SG_ALERT (SG_SYSTEMS, SG_ALERT, "processing " << ConfigName);
  Val = Config.Get ("server.name");
  if (Val != "")
  {
    Servant.SetServerName (Val);
  }
  Val = Config.Get ("server.address");
  if (Val != "")
  {
    Servant.SetBindAddress (Val);
  }
  Val = Config.Get ("server.port");
  if (Val != "")
  {
    Servant.SetDataPort (StrToNum<int> (Val.c_str (), E));
    if (E)
    {
      SG_ALERT (SG_SYSTEMS, SG_ALERT,
        "invalid value for DataPort: '" << optarg << "'");
      exit (1);
    }
  }
  Val = Config.Get ("server.telnet_port");
  if (Val != "")
  {
    Servant.SetTelnetPort (StrToNum<int> (Val.c_str (), E));
    if (E)
    {
      SG_ALERT (SG_SYSTEMS, SG_ALERT,
        "invalid value for TelnetPort: '" << optarg << "'");
      exit (1);
    }
  }
  Val = Config.Get("server.out_of_reach");
  if (Val != "")
  {
    Servant.SetOutOfReach (StrToNum<int> (Val.c_str (), E));
    if (E)
    {
      SG_ALERT (SG_SYSTEMS, SG_ALERT,
        "invalid value for OutOfReach: '" << optarg << "'");
      exit (1);
    }
  }
  Val = Config.Get("server.playerexpires");
  if (Val != "")
  {
    Servant.SetPlayerExpires (StrToNum<int> (Val.c_str (), E));
    if (E)
    {
      SG_ALERT (SG_SYSTEMS, SG_ALERT,
        "invalid value for Expire: '" << optarg << "'");
      exit (1);
    }
  }
  Val = Config.Get ("server.logfile");
  if (Val != "")
  {
    Servant.SetLogfile (Val);
  }
  Val = Config.Get ("server.daemon");
  if (Val != "")
  {
    if ((Val == "on") || (Val == "true"))
    {
      RunAsDaemon = true;
    }
    else if ((Val == "off") || (Val == "false"))
    {
      RunAsDaemon = false;
    }
    else
    {
      SG_ALERT (SG_SYSTEMS, SG_ALERT,
        "unknown value for 'server.daemon'!" << " in file " << ConfigName);
    }
  }
  Val = Config.Get ("server.tracked");
  if (Val != "")
  {
    string  Server;
    int     Port;
    bool    tracked;
    if (Val == "true")
    {
      tracked = true;
    }
    else
    {
      tracked = false;
    }
    Server = Config.Get ("server.tracking_server");
    Val = Config.Get ("server.tracking_port");
    Port = StrToNum<int> (Val.c_str (), E);
    if (E)
    {
      SG_ALERT (SG_SYSTEMS, SG_ALERT,
        "invalid value for tracking_port: '" << Val << "'");
      exit (1);
    }
    Servant.AddTracker (Server, Port, tracked);
  }
  Val = Config.Get ("server.is_hub");
  if (Val != "")
  {
    if (Val == "true")
    {
      Servant.SetHub (true);
    }
    else
    {
      Servant.SetHub (false);
    }
  }
  //////////////////////////////////////////////////
  //      read the list of relays
  //////////////////////////////////////////////////
  bool    MoreToRead  = true;
  string  Section = "relay";
  string  Var;
  string  Server = "";
  int     Port   = 0;

  if (! Config.SetSection (Section))
  {
    MoreToRead = false;
  }
  while (MoreToRead)
  {
    Var = Config.GetName ();
    Val = Config.GetValue();
    if (Var == "relay.host")
    { 
      Server = Val;
    }
    if (Var == "relay.port")
    { 
      Port = StrToNum<int> (Val.c_str(), E);
      if (E)
      { 
        SG_ALERT (SG_SYSTEMS, SG_ALERT,
            "invalid value for RelayPort: '" << Val << "'");
        exit (1);
      }
    }
    if ((Server != "") && (Port != 0))
    { 
      Servant.AddRelay (Server, Port);
      Server = "";
      Port   = 0;
    }
    if (Config.SecNext () == 0)
    { 
      MoreToRead = false;
    }
  }
  //////////////////////////////////////////////////
  //      read the list of crossfeeds
  //////////////////////////////////////////////////
  MoreToRead  = true;
  Section = "crossfeed";
  Var    = "";
  Server = "";
  Port   = 0;
  if (! Config.SetSection (Section))
  {
    MoreToRead = false;
  }
  while (MoreToRead)
  {
    Var = Config.GetName ();
    Val = Config.GetValue();
    if (Var == "crossfeed.host")
    {
      Server = Val;
    }
    if (Var == "crossfeed.port")
    {
      Port = StrToNum<int> (Val.c_str(), E);
      if (E)
      {
        SG_ALERT (SG_SYSTEMS, SG_ALERT,
            "invalid value for crossfeed.port: '" << Val << "'");
        exit (1);
      }
    }
    if ((Server != "") && (Port != 0))
    {
      Servant.AddCrossfeed (Server, Port);
      Server = "";
      Port   = 0;
    }
    if (Config.SecNext () == 0)
    {
      MoreToRead = false;
    }
  }
  //////////////////////////////////////////////////
  //      read the list of blacklisted IPs
  //////////////////////////////////////////////////
  MoreToRead  = true;
  Section = "blacklist";
  Var    = "";
  Val    = "";
  if (! Config.SetSection (Section))
  {
    MoreToRead = false;
  }
  while (MoreToRead)
  {
    Var = Config.GetName ();
    Val = Config.GetValue();
    if (Var == "blacklist")
    {
      Servant.AddBlacklist (Val);
    }
    if (Config.SecNext () == 0)
    {
      MoreToRead = false;
    }
  }
  //////////////////////////////////////////////////
  return (true);
} // ProcessConfig ( const string& ConfigName )
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
//      parse commandline parameters
//
//////////////////////////////////////////////////////////////////////
int
ParseParams ( int argcount, char* argvars[] )
{
  int     m;
  int     E;

  while ((m=getopt(argcount,argvars,"a:c:dDhl:o:p:t:v:")) != -1) 
  {
    switch (m)
    {
      case 'h':
        cerr << endl;
        cerr << "syntax: " << argvars[0] << " options" << endl;
        PrintHelp ();
        break; // never reached
      case 'a':
        Servant.SetTelnetPort (StrToNum<int> (optarg, E));
        if (E)
        {
          cerr << "invalid value for TelnetPort: '"
               << optarg << "'" << endl;
          exit(1);
        }
        break;
      case 'c':
        ConfigFileName = optarg;
        break;
        if (ProcessConfig (optarg) == false)
        {
          cerr << "could not read '"
               << optarg << "' for input!" 
               << endl;
          exit (1);
        }
        break;
      case 'p':
        Servant.SetDataPort (StrToNum<int>  (optarg, E));
        if (E)
        {
          cerr << "invalid value for DataPort: '"
               << optarg << "'" << endl;
          exit(1);
        }
        break;
      case 'o':
        Servant.SetOutOfReach (StrToNum<int>  (optarg, E));
        if (E)
        {
          cerr << "invalid value for OutOfReach: '"
               << optarg << "'" << endl;
          exit(1);
        }
        break;
      case 'v':
        Servant.SetLoglevel (StrToNum<int>  (optarg, E));
        if (E)
        {
          cerr << "invalid value for Loglevel: '"
               << optarg << "'" << endl;
          exit(1);
        }
        break;
      case 't':
        Servant.SetPlayerExpires (StrToNum<int>  (optarg, E));
        if (E)
        {
          cerr << "invalid value for expire: '"
               << optarg << "'" << endl;
          exit(1);
        }
        break;
      case 'l':
        Servant.SetLogfile (optarg);
        break;
      case 'd':
        RunAsDaemon = false;
        break;
      case 'D':
        RunAsDaemon = true;
        break;
      default:
        cerr << endl << endl;
        PrintHelp ();
        exit (1);
      } // switch ()
    } // while ()
  return (1); // success
} // ParseParams()
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
//      read config files
//
//////////////////////////////////////////////////////////////////////
void
ReadConfigs ( bool ReInit = false )
{
  string Path;

  Path = SYSCONFDIR;
  Path += "/fgms.conf";
  if (ProcessConfig (ConfigFileName) == true)
    return;
  if (ProcessConfig (Path) == true)
    return;
  Path = getenv ("HOME");
  if (Path != "")
  {
    Path += "/fgms.conf";
    if (ProcessConfig (Path))
        return;
  }
  ProcessConfig ("fgms.conf");
} // ReadConfigs ()
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
//  if we receive a SIGHUP, reinit application
//
//////////////////////////////////////////////////////////////////////
void SigHUPHandler ( int SigType )
{
  Servant.PrepareInit();
  ReadConfigs (true);
  if (Servant.Init () != 0)
  {
    SG_ALERT (SG_SYSTEMS, SG_ALERT, "received HUP signal, but reinit failed!");
    exit (1);
  }
  signal (SigType, SigHUPHandler);
} // SigHUPHandler ()
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
//  add the pid of the child to the main exit pon receiving SIGCHLD
//
//////////////////////////////////////////////////////////////////////
void
SigCHLDHandler (int s)
{
  while (waitpid (-1, NULL, WNOHANG) > 0)
    /* intentionally empty */ ;
} // SigCHLDHandler ()
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
//      MAIN routine
//
//////////////////////////////////////////////////////////////////////
int
main ( int argc, char* argv[] )
{
  int     I;
  struct  sigaction sig_child;

#if defined ENABLE_DEBUG
//  logbuf::set_log_classes(SG_GENERAL);
#endif
  // SIGHUP
  signal (SIGHUP, SigHUPHandler);
  // SIGCHLD
  sig_child.sa_handler = SigCHLDHandler;
  sigemptyset (&sig_child.sa_mask);
  sig_child.sa_flags = SA_RESTART;
  if (sigaction (SIGCHLD, &sig_child, NULL) == -1)
  {
    exit(1);
  }
  ParseParams (argc, argv);
  ReadConfigs ();
  sglog().setLogLevels( SG_ALL, SG_INFO );
  sglog().enable_with_date (true);
  I = Servant.Init ();
  if (I != 0)
  {
    Servant.CloseTracker();
    return (I);
  }
  if (RunAsDaemon)
  {
    Myself.Daemonize ();
  }
  I = Servant.Loop();
  if (I != 0)
  {
    Servant.CloseTracker();
    return (I);
  }
  Servant.Done();
  return (0);
} // main()
//////////////////////////////////////////////////////////////////////

// vim: ts=2:sw=2:sts=0
