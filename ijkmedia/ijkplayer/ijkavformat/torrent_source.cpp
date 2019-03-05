#include "internal.h"

//#ifdef USE_TORRENT
#include <iostream>
#include <fstream>
#include <pthread.h>
//#include <boost/filesystem.hpp>
#include "torrent_source.h"
#include "live_storage.h"
//#include "libtorrent/escape_string.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/fingerprint.hpp"
#include "libtorrent/magnet_uri.hpp"
#include "libtorrent/session_settings.hpp"
#include "libtorrent/read_resume_data.hpp"
#include "libtorrent/write_resume_data.hpp"
#ifdef WIN32
// #pragma comment(lib, "../openssl/libs/libeay32.lib");
// #pragma comment(lib, "../openssl/libs/ssleay32.lib");
#endif // WIN32

#ifndef AVSEEK_SIZE
#define AVSEEK_SIZE 0x10000
#endif


//-------------------------------------------------------------------------
bool load_file(std::string const& filename, std::vector<char>& v
               , int limit = 8000000)
{
    std::fstream f(filename, std::ios_base::in | std::ios_base::binary);
    f.seekg(0, std::ios_base::end);
    auto const s = f.tellg();
    if (s > limit || s < 0) return false;
    f.seekg(0, std::ios_base::beg);
    v.resize(static_cast<std::size_t>(s));
    if (s == std::fstream::pos_type(0)) return !f.fail();
    f.read(v.data(), v.size());
    return !f.fail();
}

int save_file(std::string const& filename, std::vector<char> const& v)
{
    std::fstream f(filename, std::ios_base::trunc | std::ios_base::out | std::ios_base::binary);
    f.write(v.data(), v.size());
    return !f.fail();
}

std::string to_hex(lt::sha1_hash const& s)
{
    std::stringstream ret;
    ret << s;
    return ret.str();
}

std::string path_append(std::string const& lhs, std::string const& rhs)
{
    if (lhs.empty() || lhs == ".") return rhs;
    if (rhs.empty() || rhs == ".") return lhs;
    
#if defined(TORRENT_WINDOWS) || defined(TORRENT_OS2)
#define TORRENT_SEPARATOR "\\"
    bool need_sep = lhs[lhs.size()-1] != '\\' && lhs[lhs.size()-1] != '/';
#else
#define TORRENT_SEPARATOR "/"
    bool need_sep = lhs[lhs.size()-1] != '/';
#endif
    return lhs + (need_sep?TORRENT_SEPARATOR:"") + rhs;
}

std::string resume_file(std::string save_path, lt::sha1_hash const& info_hash)
{
    return path_append(save_path, path_append(".resume"
                                              , to_hex(info_hash) + ".resume"));
}
//=========================================================================

torrent_source::torrent_source()
	: m_abort(false)
	, m_reset(false)
    , m_tid_alert(0)
{
	m_current_video.index = -1;
}

torrent_source::~torrent_source()
{
	close();
}

lt::storage_interface* live_storage_constructor(lt::storage_params const& params, lt::file_pool& pool)
{
    return new live_storage(params,pool);
}

bool torrent_source::open(boost::any ctx)
{
	// 保存打开指针.
	m_open_data.reset(boost::any_cast<open_torrent_data*>(ctx));

	// 开启下载对象.
	add_torrent_params p;

	error_code ec;

	// 打开种子, 开始下载.
	if (m_open_data->is_file)
	{
		p.ti = std::make_shared<torrent_info>(m_open_data->filename, ec);
		if (ec)
		{
			printf("%s\n", ec.message().c_str());
			return false;
		}
        
        std::vector<char> resume_data;
        if (load_file(resume_file(m_open_data->save_path,p.ti->info_hash()), resume_data))
        {
            p = lt::read_resume_data(resume_data, ec);
            if (ec) std::printf("  failed to load resume data: %s\n", ec.message().c_str());
        }
        ec.clear();
	}
	// 非文件的种子, magnet
	else if (!m_open_data->is_file)
	{
		p = lt::parse_magnet_uri(m_open_data->filename, ec);
        //p.url = m_open_data->filename;
		if (ec)
		{
			printf("%s\n", ec.message().c_str());
			return false;
		}
        
        std::vector<char> resume_data;
        if (load_file(resume_file(m_open_data->save_path,p.info_hash), resume_data))
        {
            p = lt::read_resume_data(resume_data, ec);
            if (ec) std::printf("  failed to load resume data: %s\n", ec.message().c_str());
        }
        ec.clear();
	}
    p.save_path = m_open_data->save_path;
    p.storage = live_storage_constructor;

	/*m_session.add_dht_router(std::make_pair(   //lihengz
		std::string("router.bittorrent.com"), 6881));
	m_session.add_dht_router(std::make_pair(
		std::string("router.utorrent.com"), 6881));
	m_session.add_dht_router(std::make_pair(
		std::string("router.bitcomet.com"), 6881));
    const std::pair<std::string, int> trNode("dht.transmissionbt.com",6881);
    m_session.add_dht_router(trNode);
    const std::pair<std::string, int> aeNode("dht.aelitis.com",6881);
    m_session.add_dht_router(aeNode);
	m_session.start_dht();*/
    //m_session.start_lsd();
    //m_session.start_upnp();
    //m_session.start_natpmp();
	//    m_session.load_asnum_db("GeoIPASNum.dat");
	//    m_session.load_country_db("GeoIP.dat");
	//m_session.listen_on(std::make_pair(6881, 6891),ec);

	// 设置缓冲.
	/*session_settings settings = m_session.settings();
	settings.use_read_cache = false;
	settings.disk_io_read_mode = session_settings::disable_os_cache;
	settings.broadcast_lsd = true;
	settings.allow_multiple_connections_per_ip = true;
	settings.local_service_announce_interval = 15;
	settings.min_announce_interval = 20;
    settings.no_recheck_incomplete_resume = true; //lihengz
	m_session.set_settings(settings);*/
    
    settings_pack pset;
    pset.set_int(settings_pack::alert_mask
                  , alert::error_notification
                 | alert::peer_notification
                 | alert::port_mapping_notification
                 | alert::storage_notification
                 | alert::tracker_notification
                 | alert::connect_notification
                 | alert::status_notification
                 | alert::ip_block_notification
                 | alert::performance_warning
                 | alert::dht_notification
                 | alert::incoming_request_notification
                 | alert::dht_operation_notification
                 | alert::port_mapping_log_notification
                 | alert::file_progress_notification);
    pset.set_bool(settings_pack::no_recheck_incomplete_resume,true);
    pset.set_str(settings_pack::dht_bootstrap_nodes,"dht.libtorrent.org:25401,router.bittorrent.com:6881,router.utorrent.com:6881,dht.transmissionbt.com:6881,dht.aelitis.com:6881");
    m_session.apply_settings(pset);
    
    dht::dht_settings dhtset = m_session.get_dht_settings();
    dhtset.read_only = true;
    dhtset.privacy_lookups = true;
    m_session.set_dht_settings(dhtset);

	// 添加到session中.
	m_torrent_handle = m_session.add_torrent(std::move(p), ec);
	if (ec)
	{
		printf("%s\n", ec.message().c_str());
		return false;
	}

    //m_torrent_handle.force_recheck();//lihengz
	//m_torrent_handle.force_reannounce(); todo

	// 自定义播放模式下载.
	//m_torrent_handle.set_user_defined_download(true);  //lihengz
    m_torrent_handle.set_sequential_download(true);

	// 限制上传速率为测试.
	// m_session.set_upload_rate_limit(80 * 1024);
	// m_session.set_local_upload_rate_limit(80 * 1024);

	// 创建bt数据读取对象.
	m_read_op.reset(new extern_read_op(m_torrent_handle, m_session));
	//m_abort = false;
    
    start_alert_process();
    
    while (!m_torrent_handle.has_metadata()) {
        if(m_abort) {
            printf("abort in open\n");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    int index = 0;
    int fileno = 0;
    // 遍历视频文件.
    const file_storage &fs = m_torrent_handle.torrent_file()->files();
    for (file_storage::iterator iter = fs.begin();
         iter != fs.end(); iter++)
    {
        std::string path = iter->filename().to_string(); //convert_to_native  lihengz
        std::string ext = path.substr(path.find_last_of('.') );
        if (ext == ".rmvb" ||
            ext == ".wmv" ||
            ext == ".avi" ||
            ext == ".mkv" ||
            ext == ".flv" ||
            ext == ".rm" ||
            ext == ".mp4" ||
            ext == ".3gp" ||
            ext == ".webm" ||
            ext == ".mpg")
        {
            video_file_info vfi;
            vfi.filename = iter->filename().to_string(); //convert_to_native lihengz
            vfi.base_offset = iter->offset;
            vfi.offset = iter->offset;
            vfi.data_size = iter->size;
            vfi.index = index++;
            vfi.status = 0;
            vfi.fileno =fileno;
            
            // 当前视频默认置为第一个视频.
            if (m_current_video.index == -1)
                m_current_video = vfi;
            
            // 保存到视频列表中.
            m_videos.push_back(vfi);
        }
        
        fileno++;
    }
    
    std::vector<int> files;
    files = m_torrent_handle.file_priorities();
    std::for_each(files.begin(), files.end(), boost::lambda::_1 = 0);
    files[m_current_video.fileno] = 7;
    m_torrent_handle.prioritize_files(files);

	return true;
}

bool torrent_source::handle_alert(lt::alert* a)
{
    using namespace lt;
    if(read_piece_alert* p = alert_cast<read_piece_alert>(a))
    {
        if (!m_read_op || m_videos.size() == 0)
            return false;
        torrent_handle h = p->handle;
        if (!p->buffer) {
            // read_piece failed
            m_read_op->on_read(NULL,0,0);
        }
        // use p
        const torrent_info& info = h.get_torrent_info();
        boost::int64_t total_offset=(boost::int64_t)p->piece * (boost::int64_t)info.piece_length();
        m_read_op->on_read(p->buffer.get(),total_offset,p->size);
    }
    else if (metadata_received_alert* p = alert_cast<metadata_received_alert>(a))
    {
        torrent_handle h = p->handle;
        h.save_resume_data(torrent_handle::save_info_dict);
        
        std::shared_ptr<const torrent_info> tf = h.torrent_file();
        //++num_outstanding_resume_data;
    }
    else if (add_torrent_alert* p = alert_cast<add_torrent_alert>(a))
    {
        if (p->error)
        {
            std::fprintf(stderr, "failed to add torrent: %s %s\n"
                         , p->params.ti ? p->params.ti->name().c_str() : p->params.name.c_str()
                         , p->error.message().c_str());
        }
        else
        {
            torrent_handle h = p->handle;
            
            h.save_resume_data(torrent_handle::save_info_dict | torrent_handle::only_if_modified);
            //++num_outstanding_resume_data;
        }
    }
    else if (torrent_finished_alert* p = alert_cast<torrent_finished_alert>(a))
    {
        //p->handle.set_max_connections(max_connections_per_torrent / 2);
        
        // write resume data for the finished torrent
        // the alert handler for save_resume_data_alert
        // will save it to disk
        torrent_handle h = p->handle;
        h.save_resume_data(torrent_handle::save_info_dict);
        //++num_outstanding_resume_data;
    }
    else if (save_resume_data_alert* p = alert_cast<save_resume_data_alert>(a))
    {
       // --num_outstanding_resume_data;
        torrent_handle h = p->handle;
        auto const buf = write_resume_data_buf(p->params);
        torrent_status st = h.status(torrent_handle::query_save_path);
        save_file(resume_file(m_open_data->filename, st.info_hash), buf);
    }
    else if (save_resume_data_failed_alert* p = alert_cast<save_resume_data_failed_alert>(a))
    {
        //--num_outstanding_resume_data;
        // don't print the error if it was just that we didn't need to save resume
        // data. Returning true means "handled" and not printed to the log
        return p->error == lt::errors::resume_data_not_modified;
    }
    else if (torrent_paused_alert* p = alert_cast<torrent_paused_alert>(a))
    {
        // write resume data for the finished torrent
        // the alert handler for save_resume_data_alert
        // will save it to disk
        torrent_handle h = p->handle;
        h.save_resume_data(torrent_handle::save_info_dict);
        //++num_outstanding_resume_data;
    }
    if (torrent_need_cert_alert* p = alert_cast<torrent_need_cert_alert>(a))
    {
        printf("peer need ssl cert\n");
    }
    return false;
}

void torrent_source::alert_thread()
{
    std::vector<lt::alert*> alerts;
    while(!m_abort)
    {
        m_session.pop_alerts(&alerts);
        for (auto a : alerts)
        {
            //std::cout << a->message() << std::endl;
            //if (handle_alert(a)) continue;
            handle_alert(a);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void *threadFunc(void *arg)
{
    torrent_source *p_source=(torrent_source *)arg;
    p_source->alert_thread();
    
    return 0;
}

int torrent_source::start_alert_process()
{
    //boost::function0< void> f =  boost::bind(&torrent_source::alert_thread,this);
    //或boost::function<void()> f = boost::bind(&torrent_source::alert_thread,this);
    //pthread_t tidp;
    pthread_create(&m_tid_alert,NULL,threadFunc,(void*)this);
    return 0;
}

bool torrent_source::read_data(char* data, size_t size, size_t &read_size)
{
	if (!m_read_op || !data || m_videos.size() == 0)
		return false;

	bool ret = false;
	int piece_offset = 0;

	// 必须保证read_data函数退出后, 才能destroy这个对象!!!
	read_size = 0;
	m_reset = false;

	// 读取数据越界.
	if (m_current_video.offset >= m_current_video.base_offset + m_current_video.data_size ||
		m_current_video.offset < m_current_video.base_offset)
		return false;

	uint64_t &offset = m_current_video.offset;
	const torrent_info &info = m_torrent_handle.get_torrent_info();
	piece_offset = offset / info.piece_length();

	boost::mutex::scoped_lock lock(m_abort_mutex);
	// 读取数据.
	try
	{
		while (!m_abort && !m_reset)
		{
			boost::int64_t rs = 0;
			ret = m_read_op->read_data(data, offset, size, rs);
			if (ret)
			{
				read_size = rs;
				// 修正当前偏移位置.
				m_current_video.offset += rs;
				break;
			}
			else
			{
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
				//boost::this_thread::sleep(boost::posix_time::millisec(100)); lihengz
			}

			if (m_reset)
				return true;
		}
	}
	catch (boost::thread_interrupted& e)
	{
		printf("read thread is interrupted!\n");
		return false;
	}
	catch (...)
	{
		printf("read thread is interrupted!\n");
		return false;
	}

	return ret;
}

int64_t torrent_source::read_seek(uint64_t offset, int whence)
{
	if (!m_read_op || m_videos.size() == 0)
		return -1;

	if (offset > m_current_video.data_size || offset < 0)
		return -1;

	int64_t new_offset = m_current_video.offset - m_current_video.base_offset;

	// 计算新的偏移位置.
	switch (whence)
	{
	case SEEK_SET:	// 文件起始位置计算.
		{
			m_current_video.offset = m_current_video.base_offset + offset;
			new_offset = offset;
			if (m_current_video.offset > m_current_video.base_offset + m_current_video.data_size ||  //lihengz
				m_current_video.offset < m_current_video.base_offset)
				return -1;
		}
		break;
	case SEEK_CUR:	// 文件指针当前位置开始计算.
		{
			m_current_video.offset += offset;
			new_offset += offset;
			if (m_current_video.offset > m_current_video.base_offset + m_current_video.data_size ||  //lihengz
				m_current_video.offset < m_current_video.base_offset)
				return -1;
		}
		break;
	case SEEK_END:	// 文件尾开始计算.
		{
			m_current_video.offset = m_current_video.base_offset + m_current_video.data_size - offset;
			new_offset = m_current_video.data_size - offset;
			if (m_current_video.offset > m_current_video.base_offset + m_current_video.data_size ||  //lihengz
				m_current_video.offset < m_current_video.base_offset)
				return -1;
		}
		break;
	case AVSEEK_SIZE:
		{
			new_offset = m_current_video.data_size;
		}
		break;
	}

	return new_offset;
}

bool torrent_source::has_data(uint64_t offset)
{
	offset = m_current_video.base_offset + offset;

	torrent_status status = m_torrent_handle.status();
	const torrent_info &info = m_torrent_handle.get_torrent_info();
	int piece_length = info.piece_length();
	int piece_index = offset / piece_length;

	if (!status.pieces[piece_index])
		return true;
	if (++piece_index < status.num_pieces)
	{
		if (!status.pieces[piece_index])
			return true;
	}

	return false;
}

void torrent_source::close()
{
    m_session.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	m_abort = true;
    if(m_tid_alert) {
        pthread_join(m_tid_alert, NULL);
        m_tid_alert = 0;
    }
	boost::mutex::scoped_lock lock(m_abort_mutex);
	m_open_data.reset();
	m_read_op.reset();
}

bool torrent_source::set_current_video(int index)
{
	// 检查是否初始化及参数是否有效.
	if (!m_open_data)
		return false;

	if (index >= m_videos.size() || index < 0)
		return false;

	// 设置为当前视频.
	m_current_video = m_videos[index];

	return true;
}

bool torrent_source::get_current_video(video_file_info &vfi) const
{
	if (!m_open_data)
		return false;

	// 返回当前视频.
	vfi = m_current_video;

	return true;
}

void torrent_source::reset()
{
	m_reset = true;
}

//#endif // USE_TORRENT
