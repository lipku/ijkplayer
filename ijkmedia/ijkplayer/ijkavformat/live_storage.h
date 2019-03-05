#ifndef LIVE_STORAGE_H
#define LIVE_STORAGE_H

#include <cstdlib>
#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/fwd.hpp"

#include <iostream>

#define MAX_NUM 10

using namespace lt;
class live_storage:public default_storage
{
public:
    live_storage(storage_params const& params, file_pool&);
    ~live_storage();
    
    int writev(span<iovec_t const> bufs
               , piece_index_t piece, int offset, open_mode_t flags, storage_error& ec);
    
    int readbuf(char *buffer, piece_index_t piece, int offset, int readsize);
    
private:
    struct store_type {
        std::vector<char> data;
        bool valid;
        int piece;
    };
    store_type m_live_data[MAX_NUM];
    
    std::mutex m_mutex_buf;
    
};

#endif
