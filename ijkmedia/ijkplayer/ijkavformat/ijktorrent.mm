/*
 * Copyright (c) 2015 Bilibili
 * Copyright (c) 2015 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <assert.h>
#include "torrent_source.h"

extern "C" {
#include "libavformat/url.h"
#include "libavutil/avstring.h"
#include "ijkplayer/ijkavutil/ijkutils.h"
#include "ijkiourl.h"
}
    
#include <stdint.h>
#import <Foundation/Foundation.h>
    
typedef struct IjkIOTorrentContext {
   
    torrent_source *bt_source;
} IjkIOTorrentContext;

static int ijktorrent_open(IjkURLContext *h, const char *url, int flags, IjkAVDictionary **options)
{
    IjkIOTorrentContext *c= (IjkIOTorrentContext *)h->priv_data;
    c->bt_source = new torrent_source();
    
    int ret = -1;
    
    av_strstart(url, "torrent:", &url);
    open_torrent_data *otd = new open_torrent_data;
    // 保存torrent种子数据.
    otd->is_file = true; //false
    otd->filename = url;
    if (otd->filename.substr(0, 7) == "magnet:")
        otd->is_file = false;
    
    // 分配空间.
    /*char *dst = new char[bt_info.torrent_length];
    otd->torrent_data.reset(dst);
    
    // 复制torrent数据到open_torrent_data中.
    memcpy(dst, bt_info.torrent_data, bt_info.torrent_length);
    
    // 更新种子数据长度.
    otd->data_size = bt_info.torrent_length;*/
    
    // 得到当前路径, 并以utf8编码.
    // windows平台才需要，Linux下就是utf8,无需转化.
    //std::string ansi = url;
    //char *pindex = strrchr((char*)url,'/');
    //otd->save_path = ansi.substr(0,pindex-url+1).append("tmp2");
    
    //NSArray *documentArray = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, NO);
    NSString *documentPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject]; //NSTemporaryDirectory(); //[documentArray firstObject];
    otd->save_path = [documentPath cStringUsingEncoding:NSUTF8StringEncoding];
    
    // 得到保存路径, 如果为空, 则下载数据保存在当前目录下.
    /*if (!strlen(bt_info.save_path))
    {
        ansi = boost::filesystem::current_path().string();
        
        // 更新保存路径.
        otd->save_path = ansi;
    }
    else
    {
        ansi = std::string(bt_info.save_path);
        
#ifdef _WIN32
        ansi = avhttp::detail::ansi_utf8(ansi);
#endif
        // 更新保存路径.
        otd->save_path = ansi;
        strcpy(bt_info.save_path, ansi.c_str());
    }*/
    
    // 打开种子.
    if (c->bt_source->open(otd))
        ret=0;


    if (ret)
        goto fail;

    return 0;
fail:
    return ret;
}

static int ijktorrent_close(IjkURLContext *h)
{
    IjkIOTorrentContext *c= (IjkIOTorrentContext *)h->priv_data;
    
    c->bt_source->close();
    delete c->bt_source;
    c->bt_source = NULL;

    return 0;
}

static int ijktorrent_read(IjkURLContext *h, unsigned char *buf, int size)
{
    IjkIOTorrentContext *c= (IjkIOTorrentContext *)h->priv_data;
    size_t readbytes = 0;
    
    if (!c->bt_source->read_data((char *)buf, size, readbytes))
        return -1;

    return readbytes;
}

static int64_t ijktorrent_seek(IjkURLContext *h, int64_t offset, int whence)
{
    IjkIOTorrentContext *c= (IjkIOTorrentContext *)h->priv_data;
    
    // 如果返回true, 则表示数据不够, 需要缓冲.
    int64_t ret = c->bt_source->read_seek(offset, whence);
    
    if (whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END)
    {
        if (!c->bt_source->has_data(ret))
        {
            // 表示seek位置没有数据, 上层应该根据dl_info.not_enough判断是否暂时进行缓冲.
            //ctx->dl_info.not_enough = 1;
        }
    }


    return ret;
}

IjkURLProtocol ijkio_torrent_protocol = {
        .name                = "ijktorrent",
        .url_open2           = ijktorrent_open,
        .url_read            = ijktorrent_read,
        .url_seek            = ijktorrent_seek,
        .url_close           = ijktorrent_close,
        .priv_data_size      = sizeof(IjkIOTorrentContext),
};

