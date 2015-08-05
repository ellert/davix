/*
 * This File is part of Davix, The IO library for HTTP based protocols
 * Copyright (C) CERN 2013  
 * Author: Kwong Tat Cheung <kwong.tat.cheung@cern.ch>
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

#ifndef DAVIX_TASKQUEUE_HPP
#define DAVIX_TASKQUEUE_HPP

#include <davix.hpp>
#include <tools/davix_mutex.hpp>
#include <tools/davix_op.hpp>
#include <pthread.h>

#define DEFAULT_WAIT_TIME 30

namespace Davix{

class DavixTaskQueue{
public:
    DavixTaskQueue();
    int pushOp(DavixOp* op);
    DavixOp* popOp();
    int getSize();
    bool isEmpty();
    void shutdown();
    bool isStopped();
private:
    std::deque<DavixOp*> taskQueue;
    pthread_mutex_t tq_mutex;
    pthread_cond_t popOpConvar;
    pthread_cond_t pushOpConvar;
    bool _shutdown;
};

}

#endif // DAVIX_TASKQUEUE_HPP