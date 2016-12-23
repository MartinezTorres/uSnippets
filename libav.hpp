////////////////////////////////////////////////////////////////////////
// ENCODING IMAGES USING AVCONV
//
// Manuel Martinez (manuel.martinez@kit.edu)
//
// license: LGPLv3

#pragma once 

#include <opencv2/opencv.hpp>
#include <memory>
#include <mutex>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/mem.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/opt.h>
	#include <libavutil/channel_layout.h>
}

namespace uSnippets {
class LibAV {
	LibAV() = delete;
		
//	static std::unique_lock<std::recursive_mutex> getGlobalLock() { static std::recursive_mutex mtx; return std::unique_lock<std::recursive_mutex>(mtx); }

static int my_lockmgr_cb(void **m, enum AVLockOp op) {
	
	if (not m) return -1;
	std::mutex *mtx = (std::mutex*)(*m);
	
	if (op == AV_LOCK_CREATE) 
		*m = (void*)(new std::mutex);
	else if (op == AV_LOCK_OBTAIN)
		mtx->lock();
	else if (op == AV_LOCK_RELEASE)
		mtx->unlock();
	else if (op == AV_LOCK_DESTROY)
		delete mtx;
	else 
		return 1;
	return 0;
}
 	static void initLibAV() {
		static bool init = false;
		static std::mutex mtx;
		if (init) return;
		std::lock_guard<std::mutex> lock(mtx);
		if (init) return;
		
		av_lockmgr_register(&my_lockmgr_cb);
		avcodec_register_all();
		init = true;
	}
		

	
public:
	
	typedef std::array<std::array<int16_t,2>,1152> MP2Pkg;

	// In source, stereo 16 bits per sample, 44100Hz
	static std::string mp2encode(const std::vector<MP2Pkg> &in, int bitrate = 160000) {

		
		initLibAV();

		AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
		if (not codec) throw(std::runtime_error("could not find codec"));

		AVCodecContext *ctx = avcodec_alloc_context3(codec);
		ctx->bit_rate = bitrate;
		ctx->sample_fmt = AV_SAMPLE_FMT_S16;
		ctx->sample_rate    = 44100;
		ctx->channel_layout = AV_CH_LAYOUT_STEREO;
		ctx->channels       = 2;
		
		if (avcodec_open2(ctx, codec, NULL) < 0) throw(std::runtime_error("could not open codec"));
		
		AVFrame *frame = avcodec_alloc_frame();
		frame->nb_samples     = ctx->frame_size;
		frame->format         = ctx->sample_fmt;
		frame->channel_layout = ctx->channel_layout;
		
		std::string out;
		AVPacket avpkt;
		for (auto &inpkg : in) {
		
			av_init_packet(&avpkt);
			avpkt.data = NULL;
			avpkt.size = 0;
			
			avcodec_fill_audio_frame(frame, ctx->channels, ctx->sample_fmt, (const uint8_t *)&inpkg[0][0], sizeof(MP2Pkg), 0);
			
			int p = 0;
			if (avcodec_encode_audio2(ctx, &avpkt, frame, &p) < 0) throw(std::runtime_error("error encoding frame\n"));

			if (p) {
				out += std::string((char *)avpkt.data, avpkt.size);
				av_free_packet(&avpkt);
			}
		}
		
		avcodec_free_frame(&frame);
		avcodec_close(ctx);
		av_free(ctx);

		return out;
	}

	static std::vector<MP2Pkg> mp2decode(const std::string &in) {
		
		initLibAV();

		AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MP2);
		if (not codec) throw(std::runtime_error("could not find codec"));

		AVCodecContext *ctx = avcodec_alloc_context3(codec);
		ctx->request_sample_fmt = AV_SAMPLE_FMT_S16;
		if (avcodec_open2(ctx, codec, NULL) < 0) throw(std::runtime_error("could not open codec"));

		std::string inbuf = in + std::string(FF_INPUT_BUFFER_PADDING_SIZE, (char)0);

		AVPacket avpkt;
		av_init_packet(&avpkt);
		avpkt.data = (uint8_t*)&inbuf[0];
		avpkt.size = in.size();
		
		std::vector<MP2Pkg> out;
		AVFrame *decoded_frame = avcodec_alloc_frame();
		while (avpkt.size > 0) { 
			
			avcodec_get_frame_defaults(decoded_frame);

			int got_frame = 0;
			int len = avcodec_decode_audio4(ctx, decoded_frame, &got_frame, &avpkt);
			if (len < 0) throw(std::runtime_error("Error while decoding\n"));
			
			if (got_frame) out.emplace_back(*(const MP2Pkg *)decoded_frame->data[0]);
			
			avpkt.size -= len;
			avpkt.data += len;
		}

		avcodec_close(ctx);
		av_free(ctx);
		avcodec_free_frame(&decoded_frame);
		
		return out;
	}

	class ImgYUV480P {
		cv::Mat1b yuv;
	public:
		int rows, cols;
		ImgYUV480P(cv::Mat1b &img) : 
			yuv(img.rows / 2 * 3, img.cols),
			rows(img.rows), cols(img.cols) {
			memcpy(Y(),img.data,rows*cols);
			memset(U(),128,rows*cols/4);
			memset(V(),64,rows*cols/4);
		};
			
		ImgYUV480P(const cv::Mat3b &img) :
			rows(img.rows), cols(img.cols) {
			cv::cvtColor(img,yuv,cv::COLOR_BGR2YUV_YV12);
		};

		ImgYUV480P(int rows, int cols, uint8_t *y, int yline, uint8_t *u, int uline, uint8_t *v, int vline) : 
			yuv(rows / 2 * 3, cols),
			rows(rows), cols(cols) {
			for (int i=0; i<rows  ; i++) memcpy(Y(i),y+yline*i,cols);
			for (int i=0; i<rows/2; i++) memcpy(U(i),u+uline*i,cols/2);
			for (int i=0; i<rows/2; i++) memcpy(V(i),v+vline*i,cols/2);
		};
		
		cv::Mat1b gray() const { return cv::Mat1b(rows, cols, yuv.data); };
		cv::Mat3b bgr() const { cv::Mat3b bgr; cv::cvtColor(yuv,bgr,cv::COLOR_YUV2BGR_YV12); return bgr; };
		
		uint8_t *Y(int row=0) { row%=rows;   return yuv.data+row*cols; }
		const uint8_t *Y(int row=0) const { row%=rows;   return yuv.data+row*cols; }
		
		uint8_t *U(int row=0) { row%=rows/2; return yuv.data+rows*cols+row*cols/2; }
		const uint8_t *U(int row=0) const { row%=rows/2; return yuv.data+rows*cols+row*cols/2; }
		
		uint8_t *V(int row=0) { row%=rows/2; return yuv.data+rows*cols+rows*cols/4+row*cols/2; }
		const uint8_t *V(int row=0) const { row%=rows/2; return yuv.data+rows*cols+rows*cols/4+row*cols/2; }
	};
	
	static std::string h264encode(const std::vector<ImgYUV480P> &images, std::string crf = "18", std::string preset = "fast") { 

		if (images.empty()) return "";

		initLibAV();

		AVCodec *codec = avcodec_find_encoder(CODEC_ID_H264);
		if (not codec) throw std::runtime_error("could not find codec");
		
		AVCodecContext *ctx = avcodec_alloc_context3(codec);
		AVFrame *pic = avcodec_alloc_frame();
		
		ctx->width = images.front().cols;
		ctx->height = images.front().rows;
		ctx->pix_fmt = AV_PIX_FMT_YUV420P;
		av_opt_set(ctx->priv_data, "preset", preset.c_str(), 0);
		av_opt_set(ctx->priv_data, "crf", crf.c_str(), 0);

		if (avcodec_open2(ctx, codec, NULL) < 0) throw std::runtime_error("could not open codec");
		
		if (av_image_alloc(pic->data, pic->linesize, ctx->width, ctx->height, ctx->pix_fmt, 32) < 0) throw std::runtime_error("could not alloc raw picture buffer");
		
		pic->format = ctx->pix_fmt;
		pic->width  = ctx->width;
		pic->height = ctx->height;
		
		std::string out;
		
		AVPacket avpkt;
		for (int p=0, i=0; i<int(images.size()) or p; i++) {
			
			av_init_packet(&avpkt);
			avpkt.data = NULL;
			avpkt.size = 0;
			
			if (i<int(images.size())) {

				for (int j=0; j<images[i].rows; j++)
					memcpy(&pic->data[0][j * pic->linesize[0]], images[i].Y(j), pic->width);

				for (int j=0; j<images[i].rows/2; j++)
					memcpy(&pic->data[1][j * pic->linesize[1]], images[i].U(j), pic->width/2);

				for (int j=0; j<images[i].rows/2; j++)
					memcpy(&pic->data[2][j * pic->linesize[2]], images[i].V(j), pic->width/2);
					
				pic->pts = i;
				
				if (avcodec_encode_video2(ctx, &avpkt, pic, &p) < 0) throw std::runtime_error("error encoding frame");
			} else {
				
				if (avcodec_encode_video2(ctx, &avpkt, NULL, &p) < 0) throw std::runtime_error("error encoding frame");
			}

			if (p) {
				out += std::string((char *)avpkt.data, avpkt.size);
				av_free_packet(&avpkt);
			}
		}

		avcodec_close(ctx);
		av_free(ctx);
		av_freep(&pic->data[0]);
		avcodec_free_frame(&pic);
		
		return out;
	}

	static std::vector<ImgYUV480P> h264decode(const std::string &in) {

		initLibAV();

		AVCodec *codec = avcodec_find_decoder(CODEC_ID_H264);
		if (not codec)throw std::runtime_error("could not find codec");

		AVCodecContext *ctx = avcodec_alloc_context3(codec);
		AVFrame *picture = avcodec_alloc_frame();
		
		if (avcodec_open2(ctx, codec, NULL) < 0) throw std::runtime_error("could not open codec");

		std::vector<ImgYUV480P> out;

		AVCodecParserContext *parser = av_parser_init(ctx->codec_id);
		parser->flags |= PARSER_FLAG_ONCE;
		
		AVPacket avpkt;
		av_init_packet(&avpkt);
		for (int pic=0,l=0,p=0; p<int(in.size()-8) or pic or l; p+=l) {
			
			int pts=0, dts=0;
			l = av_parser_parse2(parser, ctx, &avpkt.data, &avpkt.size,  p!=int(in.size())?(uint8_t *)&in[p]:NULL, in.size()-p, pts, dts, AV_NOPTS_VALUE);
			
			if (avcodec_decode_video2(ctx, picture, &pic, &avpkt) < 0 ) throw std::runtime_error("Error while decoding frame");

			if (pic)
				out.emplace_back(ctx->height, ctx->width, 
					picture->data[0], picture->linesize[0], 
					picture->data[1], picture->linesize[1], 
					picture->data[2], picture->linesize[2]);
		}

		av_free_packet(&avpkt);
		av_parser_close(parser);
		avcodec_free_frame(&picture);
		avcodec_close(ctx);
		av_free(ctx);
		
		return out;
	}

};
}
#pragma GCC diagnostic pop
