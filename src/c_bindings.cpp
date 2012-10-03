#include "core.hpp"
#include <http_backend.hpp>
#include <requestparams.hpp>
#include <glibmm.h>
#include <glib.h>

namespace Davix{
//
// Main entry point
//
CoreInterface* davix_context_create(){
    return static_cast<CoreInterface*>(new Core(new NEONSessionFactory() ));
}

}

extern "C"{

// initialization
__attribute__((constructor))
void core_init(){
    if (!g_thread_supported())
      g_thread_init(NULL);
    Glib::init();
}



davix_sess_t davix_session_new(GError ** err){
    try{
        Davix::CoreInterface* comp = static_cast<Davix::CoreInterface*>(new Davix::Core(new Davix::NEONSessionFactory() ));
        return (davix_sess_t) comp;
    }catch(Glib::Error & e){
        if(err)
            g_error_copy(e.gobj());
    }catch(...){
        g_set_error(err, g_quark_from_string("davix_session_new"), ENOSYS, "unexpected error");
    }
    return NULL;
}

int davix_stat(davix_sess_t sess, const char* url, struct stat * st, GError** err){
    g_return_val_if_fail(sess != NULL,-1);

    try{
        Davix::CoreInterface* comp = static_cast<Davix::CoreInterface*>(sess);

        comp->stat(url, st);
        return 0;
    }catch(Glib::Error & e){
        if(err)
            *err= g_error_copy(e.gobj());
    }catch(std::exception & e){
        g_set_error(err, g_quark_from_string("davix_stat"), EINVAL, "unexcepted error %s", e.what());
    }
    return -1;
}



int davix_params_set_auth_callback(davix_params_t params, davix_auth_callback call, void* userdata, GError** err){
    g_return_val_if_fail(params != NULL, -1);
    int ret = -1;
    try{
        Davix::RequestParams* p = (Davix::RequestParams*)(params);
        p->set_authentification_controller(userdata, call);
        ret = 0;
    }catch(Glib::Error & e){
        if(err)
            g_error_copy(e.gobj());
    }catch(...){
        g_set_error(err, g_quark_from_string("davix_params_set_auth_callback"), EINVAL, "unexpected error");
    }
    return ret;
}


int davix_params_set_ssl_check(davix_params_t params, gboolean ssl_check, GError** err){
    g_return_val_if_fail(params != NULL, -1);
    int ret = -1;
    try{
        Davix::RequestParams* p = (Davix::RequestParams*)(params);
        p->set_ssl_ca_check(ssl_check);
        ret = 0;
    }catch(Glib::Error & e){
        if(err)
            g_error_copy(e.gobj());
    }catch(...){
        g_set_error(err, g_quark_from_string("davix_set_auth_callback"), EINVAL, "unexpected error");
    }
    return ret;
}

int davix_set_default_session_params(davix_sess_t sess, davix_params_t params, GError ** err){
    g_return_val_if_fail(params != NULL && sess != NULL , -1);
    int ret = -1;
    try{
        Davix::RequestParams* p = (Davix::RequestParams*)(params);
        Davix::CoreInterface* comp = static_cast<Davix::CoreInterface*>(sess);
        comp->getSessionFactory()->set_parameters(*p);
        ret = 0;
    }catch(Glib::Error & e){
        if(err)
            g_error_copy(e.gobj());
    }catch(...){
        g_set_error(err, g_quark_from_string("davix_set_auth_callback"), EINVAL, "unexpected error");
    }
    return ret;
}



void davix_session_free(davix_sess_t sess){
    if(sess != NULL)
        delete static_cast<Davix::CoreInterface*>(sess);
}

davix_params_t davix_params_new(){
    return (struct davix_request_params*) new Davix::RequestParams();
}

davix_params_t davix_params_copy(davix_params_t p){
    return (struct davix_request_params*) new Davix::RequestParams(*(Davix::RequestParams*) p);
}

void davix_params_free(davix_params_t p){
    if(p){
        delete (Davix::RequestParams*) p;
    }
}


}


