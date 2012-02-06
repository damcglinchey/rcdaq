
#include "rcdaq_rpc.h"
#include "rcdaq_actions.h"

#include "rcdaq.h"
#include "daq_device.h" 
#include "rcdaq_plugin.h" 
#include "all.h" 


#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <rpc/pmap_clnt.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "pthread.h"
#include "signal.h"

#include <vector>


#include <set>
#include <iostream>

using namespace std;

static pthread_mutex_t M_output;

std::vector<RCDAQPlugin *> pluginlist;



void rcdaq_1(struct svc_req *rqstp, register SVCXPRT *transp);


//-------------------------------------------------------------

int server_setup(int argc, char **argv)
{
  return 0;
}

//-------------------------------------------------------------

void plugin_register(RCDAQPlugin * p )
{
  pluginlist.push_back(p);
}

void plugin_unregister(RCDAQPlugin * p )
{
  // do nothing for now
  // we need to implement this once we add unloading of plugins
  // as a feature (don't see why at this point)
}

int daq_load_plugin( const char *sharedlib, std::ostream& os)
{
  
  void * voidpointer = dlopen(sharedlib, RTLD_GLOBAL | RTLD_NOW);
  if (!voidpointer) 
    {
      std::cout << "Loading of the plugin " 
		<< sharedlib << " failed: " << dlerror() << std::endl;
      os << "Loading of the plugin " 
		<< sharedlib << " failed: " << dlerror() << std::endl;
      return -1;
      
    }
  os << "Plugin " << sharedlib << " successfully loaded" << std::endl;
  return 0;

}

//-------------------------------------------------------------

// for certain flags we add some info to the status output
// with this routine. It is called from daq_status. 

int daq_status_plugin (const int flag, std::ostream& os )
{

  // in case we do have plugins, list them
  // if not, we just say "no plugins loded"
  if (   pluginlist.size() )
    {
      os << "List of loaded Plugins:" << endl;
    }
  else
    {
      os << "No Plugins loaded" << endl;
    }


  std::vector<RCDAQPlugin *>::iterator it;

  for ( it=pluginlist.begin(); it != pluginlist.end(); ++it)
    {
      (*it)->identify(os, flag);
    }
  return 0;
}

//-------------------------------------------------------------


shortResult * r_create_device_1_svc(deviceblock *db, struct svc_req *rqstp)
{
  static shortResult  result, error;

  error.str     = "Device needs at least 2 parameters";
  error.content = 1;
  error.what    = 0;
  error.status  = -1;

  result.status = 0;
  result.what   = 0;
  result.content= 0;
  result.str    = " ";


  int eventtype;
  int subid;

  int ipar[16];
  int i;


  if ( db->npar < 3) return &error;
  
  error.str     = "Wrong number of parameters";

  // and we decode the event type and subid
  eventtype  = atoi ( db->argv1); // event type
  subid = atoi ( db->argv2); // subevent id


  if ( strcasecmp(db->argv0,"device_random") == 0 ) 
    {

      // this happens to be the most complex contructor part
      // so far since there are a few variants, 2-6 parameters
      switch ( db->npar)
	{
	case 3:
          add_readoutdevice ( new daq_device_random( eventtype,
                                                     subid ));
          break;

        case 4:
          add_readoutdevice ( new daq_device_random( eventtype,
                                                     subid,
						     atoi ( db->argv3)));
          break;

        case 5:
          add_readoutdevice ( new daq_device_random( eventtype,
                                                     subid, 
						     atoi ( db->argv3),
						     atoi ( db->argv4)));
          break;

        case 6:
          add_readoutdevice ( new daq_device_random( eventtype,
                                                     subid, 
						     atoi ( db->argv3),
						     atoi ( db->argv4),
						     atoi ( db->argv5)));
          break;

        case 7:
          add_readoutdevice ( new daq_device_random( eventtype,
                                                     subid, 
						     atoi ( db->argv3),
						     atoi ( db->argv4),
						     atoi ( db->argv5),
						     atoi ( db->argv6)));
          break;

       default:
          return &error;
          break;
        }

      return &result;
    }


  else if ( strcasecmp(db->argv0,"device_file") == 0 )  
    {

      if ( db->npar < 4) return &error;

      add_readoutdevice ( new daq_device_file( eventtype,
					       subid,
					       db->argv3));
      return &result;

    }



  std::vector<RCDAQPlugin *>::iterator it;

  int status;
  for ( it=pluginlist.begin(); it != pluginlist.end(); ++it)
    {
      status = (*it)->create_device(db);
      // in both of the following cases we are done here:
      if (status == 0) return &result;  // sucessfully created 
      else if ( status == 1) return &error;  // it's my device but wrong params
      // in all other cases we continue and try the next plugin
    }

  result.content=1;
  result.str= "Unknown device";
  return &result;

}

//-------------------------------------------------------------

shortResult * r_action_1_svc(actionblock *ab, struct svc_req *rqstp)
{

  static shortResult  result;

  static std::ostringstream outputstream;


  //  std::cout << ab->action << std::endl;

  pthread_mutex_lock(&M_output);
  result.str=" ";
  result.content = 0;
  result.what = 0;
  result.status = 0;

  int status;

  outputstream.str("");

  switch ( ab->action)
    {
      
    case DAQ_BEGIN:
      //  cout << "daq_begin " << ab->ipar[0] << endl;
      result.status = daq_begin (  ab->ipar[0], outputstream);
      result.str = (char *) outputstream.str().c_str();
      result.content = 1;
      pthread_mutex_unlock(&M_output);
      return &result;
      break;

    case DAQ_END:
        // cout << "daq_end "  << endl;
      result.status = daq_end(outputstream);
      result.str = (char *) outputstream.str().c_str();
      result.content = 1;
      pthread_mutex_unlock(&M_output);
      return &result;
      break;

    case DAQ_SETFILERULE:
      // cout << "daq_set_filerule " << ab->spar << endl;
      daq_set_filerule(ab->spar);
      break;

    case DAQ_OPEN:
      // cout << "daq_open " << ab->spar << endl;
      result.status = daq_open(outputstream);
      if (result.status) 
	{
	  result.str = (char *) outputstream.str().c_str();
	  result.content = 1;
	}
      pthread_mutex_unlock(&M_output);
      return &result;
      break;

    case DAQ_CLOSE:
      // cout << "daq_close "  << endl;
      result.status = daq_close(outputstream);
      if (result.status) 
	{
	  result.str = (char *) outputstream.str().c_str();
	  result.content = 1;
	}
      pthread_mutex_unlock(&M_output);
      return &result;
      break;

    case DAQ_FAKETRIGGER:
      // cout << "daq_fake_trigger "  << endl;
      result.status =  daq_fake_trigger (ab->ipar[0], ab->ipar[1]);
      break;

    case DAQ_LISTREADLIST:
      // cout << "daq_list_readlist "  << endl;
      result.status = daq_list_readlist(outputstream);
      result.str = (char *) outputstream.str().c_str();
      result.content = 1;
      pthread_mutex_unlock(&M_output);
      return &result;
      break;

    case DAQ_CLEARREADLIST:
      // cout << "daq_clear_readlist "  << endl;
      result.status = daq_clear_readlist(outputstream);
      result.str = (char *) outputstream.str().c_str();
      result.content = 1;
      pthread_mutex_unlock(&M_output);
      return &result;
      break;

    case DAQ_STATUS:

      //      cout << "daq_status "  << endl;
      result.status = daq_status(ab->ipar[0], outputstream);
      result.str = (char *) outputstream.str().c_str();
      result.content = 1;
      pthread_mutex_unlock(&M_output);
      return &result;
      break;

    case DAQ_SETMAXEVENTS:
      //  cout << "daq_setmaxevents " << ab->ipar[0] << endl;
      result.status = daq_setmaxevents (  ab->ipar[0], outputstream);
      result.str = (char *) outputstream.str().c_str();
      if (result.status) 
	{
	  result.str = (char *) outputstream.str().c_str();
	  result.content = 1;
	}
      pthread_mutex_unlock(&M_output);
      return &result;
      break;

    case DAQ_SETMAXVOLUME:
      //  cout << "daq_setmaxvolume " << ab->ipar[0] << endl;
      result.status = daq_setmaxvolume (  ab->ipar[0], outputstream);
      result.str = (char *) outputstream.str().c_str();
      if (result.status) 
	{
	  result.str = (char *) outputstream.str().c_str();
	  result.content = 1;
	}
      pthread_mutex_unlock(&M_output);
      return &result;
      break;

    case DAQ_SETMAXBUFFERSIZE:
      //  cout << "daq_setmaxvolume " << ab->ipar[0] << endl;
      result.status = daq_setmaxbuffersize (  ab->ipar[0], outputstream);
      result.str = (char *) outputstream.str().c_str();
      if (result.status) 
	{
	  result.str = (char *) outputstream.str().c_str();
	  result.content = 1;
	}
      pthread_mutex_unlock(&M_output);
      return &result;
      break;


    case DAQ_ELOG:
      // cout << "daq_elog " << ab->spar << "  " << ab->ipar[0] << endl;
      daq_set_eloghandler( ab->spar,  ab->ipar[0], ab->spar2);
      break;

    case DAQ_LOAD:
      // 
      result.status = daq_load_plugin( ab->spar, outputstream );
      result.str = (char *) outputstream.str().c_str();
      if (result.status) 
	{
	  result.content = 1;
	}
      pthread_mutex_unlock(&M_output);
      return &result;
      break;


    default:
      result.str =   "Unknown action";
      result.content = 1;
      result.status = 1;
      break;

    }
  
  pthread_mutex_unlock(&M_output);
  return &result;
  

}

shortResult * r_shutdown_1_svc(void *x, struct svc_req *rqstp)
{

  
  static shortResult  result;

  static std::ostringstream outputstream;

  pthread_mutex_lock(&M_output);

  cout << "daq_shutdown "  << endl;

  result.str=" ";
  result.content = 0;
  result.what = 0;
  result.status = 0;

  int status;

  outputstream.str("");

  result.status = daq_shutdown ( outputstream);
  if (result.status) 
    {
      result.str = (char *) outputstream.str().c_str();
      result.content = 1;
    }
  pthread_mutex_unlock(&M_output);
  return &result;

}

//-------------------------------------------------------------


int
main (int argc, char **argv)
{

  int i;

  pthread_mutex_init(&M_output, 0); 

  rcdaq_init( M_output);


  server_setup(argc, argv);

  int servernumber = 0;

  if ( argc > 1)
    {
      servernumber = atoi(argv[1]);
    }
  std::cout << "Server number is " << servernumber << std::endl;

  register SVCXPRT *transp;
  
  pmap_unset (RCDAQ+servernumber, RCDAQ_VERS);
    
  
  transp = svctcp_create(RPC_ANYSOCK, 0, 0);
  if (transp == NULL) {
    fprintf (stderr, "%s", "cannot create tcp service.");
    exit(1);
  }
  if (!svc_register(transp, RCDAQ+servernumber, RCDAQ_VERS, rcdaq_1, IPPROTO_TCP)) {
    fprintf (stderr, "%s", "unable to register (RCDAQ+servernumber, RCDAQ_VERS, tcp).");
    exit(1);
  }
  
  svc_run ();
  fprintf (stderr, "%s", "svc_run returned");
  exit (1);
  /* NOTREACHED */
}

