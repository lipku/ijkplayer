/*

Copyright (c) 2003, Arvid Norberg All rights reserved.
Copyright (c) 2013, Jack All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_EXTERN_READ_OP_HPP_INCLUDED
#define TORRENT_EXTERN_READ_OP_HPP_INCLUDED

#include "libtorrent/torrent_handle.hpp"
//#include "libtorrent/escape_string.hpp"
#include "libtorrent/peer_info.hpp"
#include "libtorrent/alert_types.hpp"

//#include <boost/lambda/lambda.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
//#include <boost/filesystem.hpp>

namespace libtorrent
{
	// 一个简单的扩展读取操作的封装, 线程操作安全.
	class extern_read_op : public boost::noncopyable
	{
	public:
        extern_read_op(torrent_handle& h, session &s);
        ~extern_read_op();

        char const* esc(char const* code);
        int peer_index(libtorrent::tcp::endpoint addr, std::vector<libtorrent::peer_info> const& peers);
		/*void print_piece(libtorrent::partial_piece_info* pp
			, libtorrent::cached_piece_info* cs
			, std::vector<libtorrent::peer_info> const& peers
                         , std::string& out);*/

public:
        bool read_data(char* data, boost::int64_t offset, boost::int64_t size, boost::int64_t& read_size);

//protected:
        void on_read(char* data, boost::int64_t offset, boost::int64_t size);

private:
	std::mutex m_notify_mutex;
	std::condition_variable m_notify;
	torrent_handle &m_handle;
	session &m_ses;
    std::string m_file_path;
	std::fstream m_file;
	char *m_current_buffer;
	boost::int64_t m_read_offset;
	boost::int64_t *m_read_size;
	boost::int64_t m_request_size;
	bool m_is_reading;
};

}

#endif // TORRENT_EXTERN_READ_OP_HPP_INCLUDED
