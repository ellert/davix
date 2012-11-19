#include "neonrequest.hpp"

#include <cstring>
#include <libs/time_utils.h>
#include <ne_redirect.h>
#include <ne_request.h>
#include <neon/neonsession.hpp>

namespace Davix {

/*
  authentification handle for neon
 */


void neon_to_davix_code(int ne_status, ne_session* sess, const std::string & scope, DavixError** err){
    StatusCode::Code code;
    std::string str;
    switch(ne_status){
        case NE_OK:
            code = StatusCode::OK;
            str= "Status Ok";
            break;
        case NE_ERROR:
             str = std::string("Neon error : ").append(ne_get_error(sess));
             code = StatusCode::ConnexionProblem;
             break;
        case NE_LOOKUP:
             code = StatusCode::NameResolutionFailure;
             str= "Domain Name resolution failed";
             break;
        case NE_AUTH:
            code = StatusCode::authentificationError;
            str=  "Authentification Failed on server";
            break;
        case NE_PROXYAUTH:
            code = StatusCode::authentificationError;
            str=  "Authentification Failed on proxy";
        case NE_CONNECT:
            code = StatusCode::ConnexionProblem;
            str= "Could not connect to server";
            break;
        case NE_TIMEOUT:
            code = StatusCode::ConnexionTimeout;
            str= "Connection timed out";
        case NE_FAILED:
            code = StatusCode::SessionCreationError;
            str=  "The precondition failed";
            break;
        case NE_RETRY:
            code = StatusCode::RedirectionNeeded;
            str= "Retry Request";
            break;
        default:
            code= StatusCode::UnknowError;
            str= "Unknow Error from libneon";
    }
    DavixError::setupError(err,scope, code, str);
}


NEONRequest::NEONRequest(NEONSessionFactory& f, const Uri & uri_req) :
    params(),
    _neon_sess(NULL),
    _req(NULL),
    _current(uri_req),
    _orig(uri_req),
    _vec(),
    _content_ptr(),
    _content_len(0),
    _content_offset(0),
    _content_body(),
    _fd_content(-1),
    _request_type("GET"),
    _f(f),
    req_started(false),
    req_running(false),
    _headers_field(){
}




NEONRequest::~NEONRequest(){
    // safe destruction of the request
    if(req_running) endRequest(NULL);
    free_request();

}

int NEONRequest::create_req(DavixError** err){
    if(_req != NULL || req_started){
        DavixError::setupError(err, davix_scope_http_request(), StatusCode::alreadyRunning, "Http request already started, Error");
        return -1;
    }

    if( pick_sess(err) < 0)
        return -1;

    _req= ne_request_create(_neon_sess->get_ne_sess(), _request_type.c_str(), _current.getPathAndQuery().c_str());
    configure_req();

    return 0;
}

int NEONRequest::pick_sess(DavixError** err){
    DavixError * tmp_err=NULL;
    _neon_sess = std::auto_ptr<NEONSession>(new NEONSession(_f, _current, params, &tmp_err) );
    if(tmp_err){
        _neon_sess.reset(NULL);
        DavixError::propagateError(err, tmp_err);
        return -1;
    }
    return 0;
}

void NEONRequest::configure_req(){
    for(size_t i=0; i< _headers_field.size(); ++i){
        ne_add_request_header(_req, _headers_field[i].first.c_str(),  _headers_field[i].second.c_str());
    }


    if(_fd_content > 0){
        ne_set_request_body_fd(_req, _fd_content, _content_offset, _content_len);
    }else if(_content_ptr && _content_len >0){
        ne_set_request_body_buffer(_req, _content_ptr, _content_len);       
    }
}

int NEONRequest::negotiate_request(DavixError** err){

    const int n_limit = 10;
    int code, status, end_status = NE_RETRY;
    int n =0;

    davix_log_debug(" ->   Davix negociate request ... ");
    if(req_started){
        DavixError::setupError(err, davix_scope_http_request(), StatusCode::alreadyRunning, "Http request already started, Error");
        davix_log_debug(" Davix negociate request ... <-");
        return -1;
    }

    req_started = req_running= true;

    while(end_status == NE_RETRY && n < n_limit){
        davix_log_debug(" ->   NEON start internal request ... ");

        if( (status = ne_begin_request(_req)) != NE_OK && status != NE_REDIRECT){
            if( status == NE_ERROR && strstr(ne_get_error(_neon_sess->get_ne_sess()), "Could not") != NULL ){ // bugfix against neon keepalive problem
               davix_log_debug("Connexion close, retry...");
               n++;
               continue;
            }

            req_started= req_running == false;
            neon_to_davix_code(status, _neon_sess->get_ne_sess(), davix_scope_http_request(),err);
            davix_log_debug(" Davix negociate request ... <-");
            return -1;
        }

        code = getRequestCode();
        switch(code){
            case 301:
            case 302:
            case 307:
                if (this->params.getTransparentRedirectionSupport()) {
                    if( end_status != NE_OK
                            && end_status != NE_RETRY
                            && end_status != NE_REDIRECT){
                        req_started= req_running = false;
                        neon_to_davix_code(status, _neon_sess->get_ne_sess(), davix_scope_http_request(),err);
                        davix_log_debug(" Davix negociate request ... <-");
                        return -1;
                    }
                    ne_discard_response(_req);              // Get a valid redirection, drop request content
                    end_status = ne_end_request(_req);      // submit the redirection
                    if(redirect_request(err) <0){           // accept redirection
                        davix_log_debug(" Davix negociate request ... <-");
                        return -1;
                    }
                    end_status = NE_RETRY;
                }
                else {
                    end_status = 0;
                }
                break;
            case 401: // authentification requested, do retry
                ne_discard_response(_req);
                end_status = ne_end_request(_req);
                if( end_status != NE_RETRY){
                    req_started= req_running = false;
                    neon_to_davix_code(status, _neon_sess->get_ne_sess(), davix_scope_http_request(),err);
                    return -1;
                }
                davix_log_debug(" ->   NEON receive %d code, %d .... request again ... ", code, end_status);
                break;
            default:
                end_status = 0;
                break;

        }
        n++;
    }

    if(n >= n_limit){
        DavixError::setupError(err,davix_scope_http_request(),StatusCode::authentificationError, "overpass the maximum number of authentification try, cancel");
        return -2;
    }
    davix_log_debug(" Davix negociate request ... <-");
    return 0;
}

int NEONRequest::redirect_request(DavixError **err){
    const ne_uri * new_uri = ne_redirect_location(_neon_sess->get_ne_sess());
    if(!new_uri){
        DavixError::setupError(err, davix_scope_http_request(), StatusCode::UriParsingError, "Impossible to get the new redirected destination");
        return -1;
    }

    char* dst_uri = ne_uri_unparse(new_uri);
    davix_log_debug("redirection from %s://%s/%s to %s", ne_get_scheme(_neon_sess->get_ne_sess()),
                      ne_get_server_hostport(_neon_sess->get_ne_sess()), _current.getPathAndQuery().c_str(), dst_uri);

    // setup new path & session target
    _current= Uri(dst_uri);
    ne_free(dst_uri);

    // recycle old request and session
    _neon_sess.reset(NULL);
    free_request();

    // renew request
    req_started = false;
    // create a new couple of session + request
    if( create_req(err) <0){
        return -1;
    }
    req_started= true;
    return 0;
}

int NEONRequest::executeRequest(DavixError** err){
    ssize_t read_status=1;
    DavixError* tmp_err=NULL;

    if( create_req(&tmp_err) < 0){
        DavixError::propagateError(err, tmp_err);
        return -1;
    }
    davix_log_debug(" -> NEON start synchronous  request... ");
    if( negotiate_request(&tmp_err) < 0){
        DavixError::propagateError(err, tmp_err);
        return -1;
    }

    while(read_status > 0){
        davix_log_debug(" -> NEON Read data flow... ");
        size_t s = _vec.size();
        _vec.resize(s + NEON_BUFFER_SIZE);
        read_status= ne_read_response_block(_req, &(_vec[s]), NEON_BUFFER_SIZE );
        if( read_status >= 0 && read_status != NEON_BUFFER_SIZE){
           _vec.resize(s +  read_status);
        }

    }
    // push a last NULL char for safety
    _vec.push_back('\0');

    if(read_status < 0){
        neon_to_davix_code(read_status, _neon_sess->get_ne_sess(), davix_scope_http_request(), &tmp_err);
        DavixError::propagateError(err, tmp_err);
        return -1;
    }

   if( endRequest(&tmp_err) < 0){
       DavixError::propagateError(err, tmp_err);
       return -1;
   }

    davix_log_debug(" -> End synchronous request ... ");
    return 0;
}

int NEONRequest::beginRequest(DavixError** err){
    DavixError* tmp_err=NULL;
    int ret = -1;
    ret= create_req(&tmp_err);


    if( ret >=0){
       ret= negotiate_request(&tmp_err);
    }

    if(ret <0)
        DavixError::propagateError(err, tmp_err);
    return ret;
}

ssize_t NEONRequest::readBlock(char* buffer, size_t max_size, DavixError** err){
    ssize_t read_status=-1;

    if(_req == NULL){
        DavixError::setupError(err, davix_scope_http_request(), StatusCode::alreadyRunning, "No request started");
        return -1;
    }


    read_status= ne_read_response_block(_req, buffer, max_size );
    if(read_status <0){
       DavixError::setupError(err, davix_scope_http_request(), StatusCode::ConnexionProblem, "Invalid Read in request");
       return -1;
    }
    return read_status;
}

int NEONRequest::endRequest(DavixError** err){
    int status;

    if(_req == NULL || req_running == false){
        DavixError::setupError(err, davix_scope_http_request(), StatusCode::alreadyRunning, "Http request already started, Error");
        return -1;
    }

    req_running = false;
    if( (status = ne_end_request(_req)) != NE_OK){
        DavixError* tmp_err=NULL;
        if(_neon_sess.get() != NULL)
            neon_to_davix_code(status, _neon_sess->get_ne_sess(), davix_scope_http_request(), &tmp_err);
        if(tmp_err)
            davix_log_debug("NEONRequest::endRequest -> error %d Error closing request -> %s ", tmp_err->getStatus(), tmp_err->getErrMsg().c_str());
        DavixError::clearError(&tmp_err);
    }
    return 0;
}

void NEONRequest::clear_result(){
    _vec.clear();
}



int NEONRequest::getRequestCode(){
    return ne_get_status(_req)->code;
}

const std::vector<char> & NEONRequest::getAnswerContent(){
    return _vec;
}

bool NEONRequest::getAnswerHeader(const std::string &header_name, std::string &value){
    const char* answer_content = ne_get_response_header(_req, header_name.c_str());
    if(answer_content){
        value = answer_content;
        return true;
    }
    return false;
}




void NEONRequest::setRequestBodyString(const std::string & body){
    davix_log_debug("NEONRequest : add request content of size %s ", body.c_str());
    _content_body = std::string(body);
    _content_ptr = (char*) _content_body.c_str();
    _content_len = strlen(_content_ptr);
    _fd_content = -1;
}

void NEONRequest::setRequestBodyBuffer(const void *buffer, size_t len){
    _content_ptr = (char*) buffer;
    _content_len = len;
    _fd_content = -1;
}


void NEONRequest::setRequestBodyFileDescriptor(int fd, off_t offset, size_t len){
    _fd_content = fd;
    _content_ptr = NULL;
    _content_len = len;
    _content_offset = offset;
}

void NEONRequest::free_request(){
    if(_req != NULL){
        ne_request_destroy(_req);
        _req=NULL;
    }
}

} // namespace Davix
