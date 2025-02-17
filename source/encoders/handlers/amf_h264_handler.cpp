/*
 * Modern effects for a modern Streamer
 * Copyright (C) 2020 Michael Fabian Dirks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "amf_h264_handler.hpp"
#include "../codecs/h264.hpp"
#include "../encoder-ffmpeg.hpp"
#include "amf_shared.hpp"
#include "ffmpeg/tools.hpp"

extern "C" {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4242 4244 4365)
#endif
#include <libavutil/opt.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

// Settings
#define KEY_PROFILE "H264.Profile"
#define KEY_LEVEL "H264.Level"

using namespace streamfx::encoder::ffmpeg::handler;
using namespace streamfx::encoder::codec::h264;

static std::map<profile, std::string> profiles{
	{profile::CONSTRAINED_BASELINE, "constrained_baseline"},
	{profile::MAIN, "main"},
	{profile::HIGH, "high"},
};

static std::map<level, std::string> levels{
	{level::L1_0, "1.0"}, {level::L1_0b, "1.0b"}, {level::L1_1, "1.1"}, {level::L1_2, "1.2"}, {level::L1_3, "1.3"},
	{level::L2_0, "2.0"}, {level::L2_1, "2.1"},   {level::L2_2, "2.2"}, {level::L3_0, "3.0"}, {level::L3_1, "3.1"},
	{level::L3_2, "3.2"}, {level::L4_0, "4.0"},   {level::L4_1, "4.1"}, {level::L4_2, "4.2"}, {level::L5_0, "5.0"},
	{level::L5_1, "5.1"}, {level::L5_2, "5.2"},   {level::L6_0, "6.0"}, {level::L6_1, "6.1"}, {level::L6_2, "6.2"},
};

void amf_h264_handler::adjust_info(ffmpeg_factory* factory, const AVCodec* codec, std::string& id, std::string& name,
								   std::string& codec_id)
{
	name = "AMD AMF H.264/AVC (via FFmpeg)";
	if (!amf::is_available())
		factory->get_info()->caps |= OBS_ENCODER_CAP_DEPRECATED;
}

void amf_h264_handler::get_defaults(obs_data_t* settings, const AVCodec* codec, AVCodecContext* context, bool hw_encode)
{
	amf::get_defaults(settings, codec, context);

	obs_data_set_default_int(settings, KEY_PROFILE, static_cast<int64_t>(profile::HIGH));
	obs_data_set_default_int(settings, KEY_LEVEL, static_cast<int64_t>(level::UNKNOWN));
}

bool amf_h264_handler::has_keyframe_support(ffmpeg_factory* instance)
{
	return true;
}

bool amf_h264_handler::is_hardware_encoder(ffmpeg_factory* instance)
{
	return true;
}

bool amf_h264_handler::has_threading_support(ffmpeg_factory* instance)
{
	return false;
}

bool amf_h264_handler::has_pixel_format_support(ffmpeg_factory* instance)
{
	return false;
}

void amf_h264_handler::get_properties(obs_properties_t* props, const AVCodec* codec, AVCodecContext* context,
									  bool hw_encode)
{
	if (!context) {
		this->get_encoder_properties(props, codec);
	} else {
		this->get_runtime_properties(props, codec, context);
	}
}

void amf_h264_handler::update(obs_data_t* settings, const AVCodec* codec, AVCodecContext* context)
{
	amf::update(settings, codec, context);

	{
		auto found = profiles.find(static_cast<profile>(obs_data_get_int(settings, KEY_PROFILE)));
		if (found != profiles.end()) {
			av_opt_set(context->priv_data, "profile", found->second.c_str(), 0);
		}
	}

	{
		auto found = levels.find(static_cast<level>(obs_data_get_int(settings, KEY_LEVEL)));
		if (found != levels.end()) {
			av_opt_set(context->priv_data, "level", found->second.c_str(), 0);
		} else {
			av_opt_set(context->priv_data, "level", "auto", 0);
		}
	}
}

void amf_h264_handler::override_update(ffmpeg_instance* instance, obs_data_t* settings)
{
	amf::override_update(instance, settings);
}

void amf_h264_handler::log_options(obs_data_t* settings, const AVCodec* codec, AVCodecContext* context)
{
	amf::log_options(settings, codec, context);

	DLOG_INFO("[%s]     H.264/AVC:", codec->name);
	::ffmpeg::tools::print_av_option_string2(context, context->priv_data, "profile", "      Profile",
											 [](int64_t v, std::string_view o) { return std::string(o); });
	::ffmpeg::tools::print_av_option_string2(context, context->priv_data, "level", "      Level",
											 [](int64_t v, std::string_view o) { return std::string(o); });
}

void amf_h264_handler::get_encoder_properties(obs_properties_t* props, const AVCodec* codec)
{
	amf::get_properties_pre(props, codec);

	{
		obs_properties_t* grp = obs_properties_create();
		obs_properties_add_group(props, P_H264, D_TRANSLATE(P_H264), OBS_GROUP_NORMAL, grp);

		{
			auto p = obs_properties_add_list(grp, KEY_PROFILE, D_TRANSLATE(P_H264_PROFILE), OBS_COMBO_TYPE_LIST,
											 OBS_COMBO_FORMAT_INT);
			obs_property_list_add_int(p, D_TRANSLATE(S_STATE_DEFAULT), static_cast<int64_t>(profile::UNKNOWN));
			for (auto const kv : profiles) {
				std::string trans = std::string(P_H264_PROFILE) + "." + kv.second;
				obs_property_list_add_int(p, D_TRANSLATE(trans.c_str()), static_cast<int64_t>(kv.first));
			}
		}
		{
			auto p = obs_properties_add_list(grp, KEY_LEVEL, D_TRANSLATE(P_H264_LEVEL), OBS_COMBO_TYPE_LIST,
											 OBS_COMBO_FORMAT_INT);
			obs_property_list_add_int(p, D_TRANSLATE(S_STATE_AUTOMATIC), static_cast<int64_t>(level::UNKNOWN));
			for (auto const kv : levels) {
				obs_property_list_add_int(p, kv.second.c_str(), static_cast<int64_t>(kv.first));
			}
		}
	}

	amf::get_properties_post(props, codec);
}

void amf_h264_handler::get_runtime_properties(obs_properties_t* props, const AVCodec* codec, AVCodecContext* context)
{
	amf::get_runtime_properties(props, codec, context);
}

void amf_h264_handler::migrate(obs_data_t* settings, std::uint64_t version, const AVCodec* codec,
							   AVCodecContext* context)
{
	amf::migrate(settings, version, codec, context);
}

void amf_h264_handler::override_colorformat(AVPixelFormat& target_format, obs_data_t* settings, const AVCodec* codec,
											AVCodecContext* context)
{
	target_format = AV_PIX_FMT_NV12;
}
