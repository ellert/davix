/*
 * This File is part of Davix, The IO library for HTTP based protocols
 * Copyright (C) CERN 2013
 * Author: Adrien Devresse <adrien.devresse@cern.ch>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
*/

#ifndef DAVIX_NEONREQUEST_H
#define DAVIX_NEONREQUEST_H

#include <vector>
#include <utility>
#include <memory>
#include <string>

#include <davix_internal.hpp>

#include <ne_request.h>
#include <ne_auth.h>
#include <neon/neonsessionfactory.hpp>

#include <request/httprequest.hpp>
#include <neon/neonsession.hpp>

#include "../backend/BackendRequest.hpp"

namespace Davix {

#define NEON_BUFFER_SIZE        65000
#define NEON_REDIRECT_LIMIT 5

class NEONSessionFactory;
class NEONSession;
class HttpRequest;
class NEONSessionWrapper;

class NEONRequest : public BackendRequest
{
public:
    NEONRequest(HttpRequest &h, Context& f, const Uri & uri_req);
    virtual ~NEONRequest();

    //--------------------------------------------------------------------------
    // Execute request synchronously, and store result in internal buffer.
    //--------------------------------------------------------------------------
    virtual int executeRequest(DavixError** err);

    //--------------------------------------------------------------------------
    // Major read member - implementations need to override.
    // Read a block of max_size bytes (at max) into buffer.
    //--------------------------------------------------------------------------
    virtual dav_ssize_t readBlock(char* buffer, dav_size_t max_size,DavixError** err);

    //--------------------------------------------------------------------------
    // Start request.
    //--------------------------------------------------------------------------
    virtual int beginRequest(DavixError** err);

    //--------------------------------------------------------------------------
    // Finish an already started request.
    //--------------------------------------------------------------------------
    virtual int endRequest(DavixError** err);

    //--------------------------------------------------------------------------
    // Get response status.
    //--------------------------------------------------------------------------
    virtual int getRequestCode();

    //--------------------------------------------------------------------------
    // Get a specific response header
    //--------------------------------------------------------------------------
    virtual bool getAnswerHeader(const std::string &header_name, std::string &value) const;

    //--------------------------------------------------------------------------
    // Get all response headers
    //--------------------------------------------------------------------------
    virtual size_t getAnswerHeaders(std::vector<std::pair<std::string, std::string > > & vec_headers) const;

private:

    // neon internal field
    std::unique_ptr<NEONSessionWrapper> _neon_sess;


    ne_request * _req;
    // req info
    int _number_try;
    // number of redirects so far
    int _redirects;
    // read info
    dav_ssize_t _total_read_size, _last_read;

    HttpRequest & _h;
    bool req_started, req_running;

    int _accepted_202_retries;

    ////////////////////////////////////////////
    // Private Members
    int startRequest(DavixError** err);

    int processRedirection(int neonCode, DavixError** err); // analyze and process redirection if needed

    void resetRequest();

    int instanceSession(DavixError** err);

    void configureRequest();

    void cancelSessionReuse();

    // create initial neon request object
    int createRequest(DavixError** err);

    // negociate the request : authentification, redirection, name resolution
    int negotiateRequest(DavixError** err);

    // redirection logic
    int redirectRequest(DavixError** err);

    // redirecttion caching cleaning function
    bool requestCleanup();

    void freeRequest();


    void createError(int ne_status, DavixError** err);

	static ssize_t neon_body_content_provider(void* userdata, char* buffer, size_t buflen);

    static void neon_hook_pre_send(ne_request *req, void *userdata,
                                           ne_buffer *header);

    static void neon_hook_pre_rec(ne_request *req, void *userdata,
                                        const ne_status *status);


    friend class NEONSessionWrapper;
};


//
// translate a neon error code to a davix one
//
void neon_to_davix_code(int ne_status,ne_session* sess, const std::string & scope, DavixError** err);


void neon_simple_req_code_to_davix_code(int ne_status, ne_session* sess, const std::string & scope, DavixError** err);

void configureRequestParamsProto(const Uri &uri, RequestParams &params);


} // namespace Davix

#endif // DAVIX_NEONREQUEST_H
