#include "live_storage.h"

using namespace lt;
live_storage::live_storage(storage_params const& params, file_pool& pool)
            :default_storage(params,pool)
{
    
}

live_storage::~live_storage()
{
    
}

int live_storage::writev(span<iovec_t const> bufs
           , piece_index_t piece, int offset, open_mode_t flags, storage_error& ec)
{
    int ret = default_storage::writev(bufs,piece,offset,flags,ec);
    
    //m_mutex_buf.lock();
    int ret1=0;
    auto& store = m_live_data[piece % MAX_NUM];
    for (auto& b : bufs) {
        if (int(store.data.size()) < offset + b.size()) store.data.resize(offset + b.size());
        std::memcpy(store.data.data() + offset, b.data(), b.size());
        offset += int(b.size());
        ret1 += int(b.size());
    }
    store.piece = piece;
    //m_mutex_buf.unlock();
    
    return ret;
}

int live_storage::readbuf(char *buffer, piece_index_t piece, int offset, int readsize)
{
    //m_mutex_buf.lock();
    auto const store = m_live_data[piece % MAX_NUM];
    if (store.piece != piece) return 0;
    if (int(store.data.size()) <= offset) return 0;
    //int ret = 0;
    int const to_copy = std::min(readsize, int(store.data.size()));
    memcpy(buffer, store.data.data()+offset, to_copy);
    //m_mutex_buf.unlock();
    
    return to_copy;
}
