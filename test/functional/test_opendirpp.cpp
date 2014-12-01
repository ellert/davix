#include "test_opendirpp.h"


#include <davix.hpp>
#include <cstring>
#include <memory>

#include "davix_test_lib.h"

using namespace Davix;


#define MY_BUFFER_SIZE 65000




int main(int argc, char** argv){
    if( argc < 2){
        std::cout << "Usage  : " << std::endl;
        std::cout <<"\t" << argv[0] << " [url]" << std::endl;
        std::cout <<"\t" << argv[0] << " [url] [CERTIFICATE_PATH] " << std::endl;
        return 0;
    }

    davix_set_log_level(DAVIX_LOG_ALL);

    RequestParams  p;
    std::auto_ptr<Context> c( new Context());
    DavPosix pos(c.get());
    Davix::DavixError* tmp_err;

    if(argc > 2){
        configure_grid_env(argv[2], p);
    }

    DAVIX_DIR* d = pos.opendirpp(&p, argv[1], &tmp_err);

    if(d){
        struct dirent * dir = NULL;
        do{
            struct stat st;
            dir= pos.readdirpp(d, &st, &tmp_err);
            if(dir)
                std::cout << "N° " << n <<" file : " << dir->d_name <<" len : " << st.st_size << " atime: "<< st.st_atime << " mode : "<< std::oct << st.st_mode;
                std::cout << " mtime : " << st.st_mtime ;
                std::cout << " ctime : " << st.st_ctime << std::endl;
        }while(dir!= NULL);

        pos.closedirpp(d, &tmp_err);
    }

    return 0;
}

