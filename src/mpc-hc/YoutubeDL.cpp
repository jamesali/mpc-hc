/*
* (C) 2018 Nicholas Parkanyi
*
* This file is part of MPC-HC.
*
* MPC-HC is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* MPC-HC is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/
#include "stdafx.h"
#include "YoutubeDL.h"
#include "rapidjson/include/rapidjson/document.h"
#include "mplayerc.h"
#include "logger.h"

struct CUtf16JSON {
    rapidjson::GenericDocument<rapidjson::UTF16<>> d;
};

CString GetYDLExePath() {
    auto& s = AfxGetAppSettings();
    CString ydlpath;
    if (s.sYDLExePath.IsEmpty()) {
        CString appdir = PathUtils::GetProgramPath(false);
        if (CPath(appdir + _T("\\yt-dlp.exe")).FileExists()) {
            ydlpath = _T("yt-dlp.exe");
        } else {
            ydlpath = _T("youtube-dl.exe");
        }
    } else {
        ydlpath = s.sYDLExePath;
        // expand environment variables
        if (ydlpath.Find(_T('%')) >= 0) {
            wchar_t expanded_buf[MAX_PATH] = { 0 };
            DWORD req = ExpandEnvironmentStrings(ydlpath, expanded_buf, MAX_PATH);
            if (req > 0 && req < MAX_PATH) {
                ydlpath = CString(expanded_buf);
            }
        }
    }
    return ydlpath;
}

CYoutubeDLInstance::CYoutubeDLInstance()
    : idx_out(0), idx_err(0),
      buf_out(nullptr), buf_err(nullptr),
      capacity_out(0), capacity_err(0),
      pJSON(new CUtf16JSON)
{
}

CYoutubeDLInstance::~CYoutubeDLInstance()
{
    std::free(buf_out);
    std::free(buf_err);
    delete pJSON;
}

bool CYoutubeDLInstance::Run(CString url)
{
    const size_t bufsize = 2000;  //2KB initial buffer size

    /////////////////////////////
    // Set up youtube-dl process
    /////////////////////////////

    PROCESS_INFORMATION proc_info;
    STARTUPINFO startup_info;
    SECURITY_ATTRIBUTES sec_attrib;
    auto& s = AfxGetAppSettings();

    YDL_LOG(url);

    CString args = _T("\"") + GetYDLExePath() + _T("\" -J --no-warnings --youtube-skip-dash-manifest");
    if (!s.sYDLSubsPreference.IsEmpty()) {
        args.Append(_T(" --all-subs --write-sub"));
        if (s.bUseAutomaticCaptions) args.Append(_T(" --write-auto-sub"));
    }
    if (url.Find(_T("list=")) > 0) {
        args.Append(_T(" --ignore-errors --no-playlist"));
    }
    args.Append(_T(" \"") + url + _T("\""));

    ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&startup_info, sizeof(STARTUPINFO));

    //child process must inherit the handles
    sec_attrib.nLength = sizeof(SECURITY_ATTRIBUTES);
    sec_attrib.lpSecurityDescriptor = NULL;
    sec_attrib.bInheritHandle = true;

    if (!CreatePipe(&hStdout_r, &hStdout_w, &sec_attrib, bufsize)) {
        return false;
    }
    if (!CreatePipe(&hStderr_r, &hStderr_w, &sec_attrib, bufsize)) {
        return false;
    }

    startup_info.cb = sizeof(STARTUPINFO);
    startup_info.hStdOutput = hStdout_w;
    startup_info.hStdError = hStderr_w;
    startup_info.wShowWindow = SW_HIDE;
    startup_info.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

    if (!CreateProcess(NULL, args.GetBuffer(), NULL, NULL, true, 0,
                       NULL, NULL, &startup_info, &proc_info)) {
        YDL_LOG(_T("Failed to create process for YDL"));
        return false;
    }

    //we must close the parent process's write handles before calling ReadFile,
    // otherwise it will block forever.
    CloseHandle(hStdout_w);
    CloseHandle(hStderr_w);


    /////////////////////////////////////////////////////
    // Read in stdout and stderr through the pipe buffer
    /////////////////////////////////////////////////////

    buf_out = static_cast<char*>(std::malloc(bufsize));
    buf_err = static_cast<char*>(std::malloc(bufsize));
    capacity_out = bufsize;
    capacity_err = bufsize;

    HANDLE hThreadOut, hThreadErr;
    idx_out = 0;
    idx_err = 0;

    hThreadOut = CreateThread(NULL, 0, BuffOutThread, this, NULL, NULL);
    hThreadErr = CreateThread(NULL, 0, BuffErrThread, this, NULL, NULL);

    WaitForSingleObject(hThreadOut, INFINITE);
    WaitForSingleObject(hThreadErr, INFINITE);

    if (!buf_out || !buf_err) {
        throw std::bad_alloc();
    }

    //NULL-terminate the data
    char* tmp;
    if (idx_out == capacity_out) {
        tmp = static_cast<char*>(std::realloc(buf_out, capacity_out + 1));
        if (tmp) {
            buf_out = tmp;
        }
    }
    buf_out[idx_out] = '\0';

    if (idx_err == capacity_err) {
        tmp = static_cast<char*>(std::realloc(buf_err, capacity_err + 1));
        if (tmp) {
            buf_err = tmp;
        }
    }
    buf_err[idx_err] = '\0';

    DWORD exitcode;
    GetExitCodeProcess(proc_info.hProcess, &exitcode);

    CloseHandle(proc_info.hProcess);
    CloseHandle(proc_info.hThread);
    CloseHandle(hThreadOut);
    CloseHandle(hThreadErr);
    CloseHandle(hStdout_r);
    CloseHandle(hStderr_r);

    // parse output
    if (exitcode == 0 || exitcode == 1) {
        if (loadJSON()) {
            return true;
        }
    }

    if (exitcode) {
        CString err = buf_err;
        if (err.IsEmpty()) {
            if (exitcode == 0xC0000135) {
                err.Format(_T("An error occurred while running Youtube-DL\n\nYou probably forgot to install this required runtime:\nMicrosoft Visual C++ 2010 Service Pack 1 Redistributable Package (x86)"));
            } else {
                err.Format(_T("An error occurred while running Youtube-DL\n\nprocess exitcode = 0x%08x"), exitcode);
            }
        } else {
            if (err.Find(_T("ERROR: Unsupported URL")) >= 0) {
                // abort without showing error message
                return false;
            }
            err = _T("Youtube-DL error message:\n\n") + err;
        }
        AfxMessageBox(err, MB_ICONERROR, 0);
    }
    return false;
}

DWORD WINAPI CYoutubeDLInstance::BuffOutThread(void* ydl_inst)
{
    auto ydl = static_cast<CYoutubeDLInstance*>(ydl_inst);
    DWORD read;

    while (ReadFile(ydl->hStdout_r, ydl->buf_out + ydl->idx_out, ydl->capacity_out - ydl->idx_out, &read, NULL)) {
        ydl->idx_out += read;
        if (ydl->idx_out == ydl->capacity_out) {
            ydl->capacity_out *= 2;
            char* tmp = static_cast<char*>(std::realloc(ydl->buf_out, ydl->capacity_out));
            if (tmp) {
                ydl->buf_out = tmp;
            } else {
                std::free(ydl->buf_out);
                ydl->buf_out = nullptr;
                return 0;
            }
        }
    }

    return GetLastError() == ERROR_BROKEN_PIPE ? 0 : GetLastError();
}

DWORD WINAPI CYoutubeDLInstance::BuffErrThread(void* ydl_inst)
{
    auto ydl = static_cast<CYoutubeDLInstance*>(ydl_inst);
    DWORD read;

    while (ReadFile(ydl->hStderr_r, ydl->buf_err + ydl->idx_err, ydl->capacity_err - ydl->idx_err, &read, NULL)) {
        ydl->idx_err += read;
        if (ydl->idx_err == ydl->capacity_err) {
            ydl->capacity_err *= 2;
            char* tmp = static_cast<char*>(std::realloc(ydl->buf_err, ydl->capacity_err));
            if (tmp) {
                ydl->buf_err = tmp;
            } else {
                std::free(ydl->buf_err);
                ydl->buf_err = nullptr;
                return 0;
            }
        }
    }

    return GetLastError() == ERROR_BROKEN_PIPE ? 0 : GetLastError();
}

struct YDLStreamDetails {
    CString protocol;
    CString url;
    int width;
    int height;
    CString vcodec;
    CString acodec;
    CString format;
    bool has_video;
    bool has_audio;
    int vbr;
    int abr;
    int fps;
    CString format_id;
    CString language;
    bool pref_lang;
};

#define YDL_EXTRA_LOGGING 0
#define YDL_LOG_URLS      1
#define YDL_TRACE         0

bool GetYDLStreamDetails(const Value& format, YDLStreamDetails& details, bool require_video, bool require_audio_only)
{
    bool canuse = true;
    details = { _T(""), _T(""), 0, 0, _T(""), _T(""), _T(""), false, false, 0, 0, 0, _T(""), _T(""), false };

    details.url = format[_T("url")].GetString();
    if (details.url.IsEmpty()) {
        #if YDL_TRACE
        YDL_LOG(_T("empty url\n"));
        #endif
        return false;
    }

    if (format.HasMember(_T("protocol")) && !format[_T("protocol")].IsNull()) {
        details.protocol = CString(format[_T("protocol")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("vcodec")) && !format[_T("vcodec")].IsNull()) {
        details.vcodec = CString(format[_T("vcodec")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("acodec")) && !format[_T("acodec")].IsNull()) {
        details.acodec = CString(format[_T("acodec")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("format")) && !format[_T("format")].IsNull()) {
        details.format = CString(format[_T("format")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("format_id")) && !format[_T("format_id")].IsNull()) {
        details.format_id = CString(format[_T("format_id")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("language")) && !format[_T("language")].IsNull()) {
        details.language = CString(format[_T("language")].GetString()).MakeLower();
    }
    if (format.HasMember(_T("width")) && !format[_T("width")].IsNull()) {
        details.width = format[_T("width")].GetInt();
    }
    if (format.HasMember(_T("height")) && !format[_T("height")].IsNull()) {
        details.height = format[_T("height")].GetInt();
    }
    if (format.HasMember(_T("language_preference")) && !format[_T("language_preference")].IsNull()) {
        details.pref_lang = format[_T("language_preference")].GetInt() > 0;
    } 

    details.has_audio = !details.acodec.IsEmpty() && details.acodec != _T("none");
    details.has_video = !details.vcodec.IsEmpty() && details.vcodec != _T("none") || (details.width > 0) || (details.height > 0);

    if (!details.has_video && details.protocol == _T("http_dash_segments")) {
        details.has_video = details.url.Find(_T("_hd_clear")) > 0; // youtube manifest url that should have video
    }
    if (!details.has_audio && details.protocol == _T("http_dash_segments")) {
        details.has_audio = details.url.Find(_T("_audio_clear")) > 0; // youtube manifest url that should have audio
    }

    // make assumption
    if (!details.has_video && !details.has_audio) {
        if (details.vcodec != _T("none")) {
            details.has_video = true;
            details.vcodec = _T("unknown");
        }
        if (details.acodec != _T("none")) {
            details.has_audio = true;
            details.acodec = _T("unknown");
        }
    }

    if (canuse && require_video && !details.has_video) {
        canuse = false;
    }
    if (canuse && require_audio_only && (details.has_video || !details.has_audio)) {
        canuse = false;
    }

    details.vbr = details.has_video && format.HasMember(_T("vbr")) && !format[_T("vbr")].IsNull() ? (int)format[_T("vbr")].GetFloat() : 0;
    if (details.vbr == 0 && details.has_video) {
        details.vbr = format.HasMember(_T("tbr")) && !format[_T("tbr")].IsNull() ? (int)format[_T("tbr")].GetFloat() : 0;
    }
    details.fps = format.HasMember(_T("fps")) && !format[_T("fps")].IsNull() ? (int)format[_T("fps")].GetDouble() : 0;
    details.abr = details.has_audio && format.HasMember(_T("abr")) && !format[_T("abr")].IsNull() ? (int)format[_T("abr")].GetFloat() : 0;
    if (details.abr == 0 && details.has_audio) {
        details.abr = format.HasMember(_T("tbr")) && !format[_T("tbr")].IsNull() ? (int)format[_T("tbr")].GetFloat() : 0;
    }

    #if YDL_TRACE
    TRACE(_T("canuse=%d protocol=%s vcodec=%s width=%d height=%d fps=%d vbr=%d acodec=%s abr=%d formatid=%s lang=%s(p%d) url=%s\n"), canuse, static_cast<LPCWSTR>(details.protocol), static_cast<LPCWSTR>(details.vcodec), details.width, details.height, details.fps, details.vbr, static_cast<LPCWSTR>(details.acodec), details.abr, static_cast<LPCWSTR>(details.format_id), static_cast<LPCWSTR>(details.language), details.pref_lang, static_cast<LPCWSTR>(details.url));
    #endif
    #if YDL_LOG_URLS
    YDL_LOG(_T("canuse=%d protocol=%s vcodec=%s width=%d height=%d fps=%d vbr=%d acodec=%s abr=%d formatid=%s lang=%s(p%d) url=%s"), canuse, static_cast<LPCWSTR>(details.protocol), static_cast<LPCWSTR>(details.vcodec), details.width, details.height, details.fps, details.vbr, static_cast<LPCWSTR>(details.acodec), details.abr, static_cast<LPCWSTR>(details.format_id), static_cast<LPCWSTR>(details.language), details.pref_lang, static_cast<LPCWSTR>(details.url));
    #endif

    return canuse;
}

#define YDL_FORMAT_AUTO      0
#define YDL_FORMAT_H264_30   1
#define YDL_FORMAT_H264_60   2
#define YDL_FORMAT_VP9_30    3
#define YDL_FORMAT_VP9_60    4
#define YDL_FORMAT_VP9P2_30  5
#define YDL_FORMAT_VP9P2_60  6
#define YDL_FORMAT_AV1_30    7
#define YDL_FORMAT_AV1_60    8

// returns true when second is better than first
bool IsBetterYDLStream(YDLStreamDetails& first, YDLStreamDetails& second, int max_height, bool separate, int preferred_format)
{
    if (first.has_video) {
        // We want separate audio/video streams
        if (separate && first.has_audio && !second.has_audio) {
            return true;
        }

        if (!second.has_video) {
            return false;
        }

        // Video format
        CString vcodec1 = first.vcodec.Left(4);
        CString vcodec2 = second.vcodec.Left(4);
        if (vcodec1 != vcodec2) {
            // Prefer stream with known format
            if (vcodec1 == _T("unkn")) {
                return true;
            }
            if (vcodec2 == _T("unkn")) {
                return false;
            }
            // AV1
            if (vcodec1 == _T("av01")) {
                return (preferred_format != YDL_FORMAT_AV1_30 && preferred_format != YDL_FORMAT_AV1_60);
            } else {
                if (vcodec2 == _T("av01")) {
                    return (preferred_format == YDL_FORMAT_AV1_30 || preferred_format == YDL_FORMAT_AV1_60);
                }
            }
            // H.264
            if ((preferred_format == YDL_FORMAT_H264_30 || preferred_format == YDL_FORMAT_H264_60)) {
                if (vcodec1 == _T("avc1")) {
                    return false;
                } else {
                    if (vcodec2 == _T("avc1")) {
                        return true;
                    }
                }
            }
        }
        if (first.vcodec != second.vcodec) {
            // VP9P2
            if ((preferred_format == YDL_FORMAT_VP9P2_30 || preferred_format == YDL_FORMAT_VP9P2_60)) {
                if (first.vcodec == _T("vp9.2")) {
                    return false;
                } else {
                    if (second.vcodec == _T("vp9.2")) {
                        return true;
                    }
                }
                // Prefer VP9P0 over others
                if (first.vcodec.Left(3) == _T("vp9")) {
                    return false;
                } else {
                    if (second.vcodec.Left(3) == _T("vp9")) {
                        return true;
                    }
                }
            }
            // VP9
            if ((preferred_format == YDL_FORMAT_VP9_30 || preferred_format == YDL_FORMAT_VP9_60)) {
                if (first.vcodec == _T("vp9") || first.vcodec == _T("vp9.0")) {
                    return false;
                } else {
                    if (second.vcodec == _T("vp9") || second.vcodec == _T("vp9.0")) {
                        return true;
                    }
                }
            }
        }

        // Video resolution
        if (max_height > 0 && first.height > max_height && first.height > second.height) {
            return true;
        }
        if (max_height > 0 && second.height > max_height) {
            return false;
        }
        if (second.height > first.height) {
            if (max_height > 0) {
                // calculate maximum width based on 16:9 AR
                return (max_height * 16 / 9 + 1) >= second.width;
            }
            return true;
        } else {
            if (second.height == first.height) {
                if (second.width > first.width) {
                    return true;
                }
                if (second.width < first.width) {
                    return false;
                }
            } else {
                return false;
            }
        }

        // Framerate
        if (preferred_format != YDL_FORMAT_AUTO && first.fps != second.fps && first.fps > 0 && second.fps > 0) {
            if (preferred_format == YDL_FORMAT_H264_60 || preferred_format == YDL_FORMAT_VP9_60 || preferred_format == YDL_FORMAT_VP9P2_60 || preferred_format == YDL_FORMAT_AV1_60) {
                if (second.fps > first.fps) {
                    return true;
                } else if (first.fps > second.fps) {
                    return false;
                }
            } else if (preferred_format == YDL_FORMAT_H264_30 || preferred_format == YDL_FORMAT_VP9_30 || preferred_format == YDL_FORMAT_VP9P2_30 || preferred_format == YDL_FORMAT_AV1_30) {
                if (first.fps > 30 && first.fps > second.fps) {
                    return true;
                } else if (second.fps > 30 && second.fps > first.fps) {
                    return false;
                }
            }
        }
    } else if (first.has_audio) {
        if (!second.has_audio) {
            return false;
        }

        // Preferred track
        if (first.pref_lang != second.pref_lang) {
            return second.pref_lang;
        }

        // Audio format
        if (first.acodec.Left(4) == _T("opus")) {
            if (second.acodec.Left(4) != _T("opus")) {
                return false;
            }
        } else {
            if (second.acodec.Left(4) == _T("opus")) {
                return true;
            }
        }
    } else {
        if (second.has_video || second.has_audio) {
            return true;
        } else {
            if (first.format != _T("none")) {
                return false;
            } else {
                if (second.format != _T("none")) return true;
            }
        }
    }

    // Prefer HTTP protocol
    if (first.protocol.Left(4) == _T("http")) {
        if (second.protocol.Left(4) != _T("http")) {
            return false;
        }
    } else {
        if (second.protocol.Left(4) == _T("http")) {
            return true;
        }
    }

    // Bitrate
    if (first.has_video) {
        if (second.vbr > first.vbr) {
            return true;
        }
    } else {
        if (second.abr > first.abr) {
            return true;
        }
    }
    return false;
}

// find best video track
bool filterVideo(const Value& formats, YDLStreamDetails& ydl_sd, int max_height, bool separate, int preferred_format)
{
    YDLStreamDetails current;
    bool found = false;
#if YDL_TRACE
    TRACE(_T("format count: %d\n"), formats.Size());
#endif
    for (rapidjson::SizeType i = 0; i < formats.Size(); i++) {
        if (GetYDLStreamDetails(formats[i], current, true, false)) {
            if (!found || IsBetterYDLStream(ydl_sd, current, max_height, separate, preferred_format)) {
                ydl_sd = current;
                #if YDL_TRACE
                TRACE(_T("This is currently best video stream\n"));
                #endif
                // A single http dash manifest can appear several times in the formats list with different video and audio parameters.
                // So if current entry does not have audio, check if another entry with same url does have audio.
                if (!ydl_sd.has_audio && ydl_sd.protocol == _T("http_dash_segments")) {
                    for (rapidjson::SizeType j = 0; j < formats.Size(); j++) {
                        if (i != j && formats[j].HasMember(_T("url")) && !formats[j][_T("url")].IsNull()) {
                            CString url = formats[j][_T("url")].GetString();
                            if (url == ydl_sd.url && formats[j].HasMember(_T("acodec")) && !formats[j][_T("acodec")].IsNull()) {
                                CString acodec = CString(formats[j][_T("acodec")].GetString()).MakeLower();
                                if (!acodec.IsEmpty() && acodec != _T("none")) {
                                    ydl_sd.has_audio = true;
                                    ydl_sd.acodec = acodec;
                                    #if YDL_TRACE
                                    TRACE(_T("Found matching audio stream for above manifest url\n"));
                                    #endif
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            found = true;
        }
    }   
    return found;
}

// find best audio track (in case we use separate streams)
bool filterAudio(const Value& formats, YDLStreamDetails& ydl_sd)
{
    YDLStreamDetails current;
    bool found = false;

    for (rapidjson::SizeType i = 0; i < formats.Size(); i++) {
        if (GetYDLStreamDetails(formats[i], current, false, true)) {
            if (!found || IsBetterYDLStream(ydl_sd, current, 0, true, 0)) {
                ydl_sd = current;
                #if YDL_TRACE
                TRACE(_T("this is currently best audio stream\n"));
                #endif
                //YDL_LOG(_T("this is currently best audio stream"));
            }
            found = true;
        }
    }
    return found;
}

bool CYoutubeDLInstance::GetHttpStreams(CAtlList<YDLStreamURL>& streams, YDLPlaylistInfo& info)
{
    CString url;
    CString extractor;
    YDLStreamDetails ydl_sd;
    YDLStreamURL stream;

    if (pJSON->d.IsObject() && pJSON->d.HasMember(_T("extractor"))) {
        extractor = pJSON->d[_T("extractor")].GetString();
    } else {
        return false;
    }

    auto& s = AfxGetAppSettings();

    if (!bIsPlaylist) {
        if (pJSON->d.HasMember(_T("title")) && !pJSON->d[_T("title")].IsNull()) {
            stream.title = pJSON->d[_T("title")].GetString();
        }

        if (!pJSON->d.HasMember(_T("formats")) || pJSON->d[_T("formats")].IsNull()) {
            if (pJSON->d.HasMember(_T("url")) && !pJSON->d[_T("url")].IsNull()) {
                stream.video_url = pJSON->d[_T("url")].GetString();
                stream.audio_url = _T("");
                if (!stream.video_url.IsEmpty()) {
                    streams.AddTail(stream);
                    return true;
                }
            }
            return false;
        }

        if (pJSON->d.HasMember(_T("series")) && !pJSON->d[_T("series")].IsNull()) stream.series = pJSON->d[_T("series")].GetString();
        if (pJSON->d.HasMember(_T("season")) && !pJSON->d[_T("season")].IsNull()) stream.season = pJSON->d[_T("season")].GetString();
        if (pJSON->d.HasMember(_T("season_number")) && !pJSON->d[_T("season_number")].IsNull()) stream.season_number = pJSON->d[_T("season_number")].GetInt();
        if (pJSON->d.HasMember(_T("season_id")) && !pJSON->d[_T("season_id")].IsNull()) stream.season_id = pJSON->d[_T("season_id")].GetString();
        if (pJSON->d.HasMember(_T("episode")) && !pJSON->d[_T("episode")].IsNull()) stream.episode = pJSON->d[_T("episode")].GetString();
        if (pJSON->d.HasMember(_T("episode_number")) && !pJSON->d[_T("episode_number")].IsNull()) stream.episode_number = pJSON->d[_T("episode_number")].GetInt();
        if (pJSON->d.HasMember(_T("episode_id")) && !pJSON->d[_T("episode_id")].IsNull()) stream.episode_id = pJSON->d[_T("episode_id")].GetString();
        if (pJSON->d.HasMember(_T("webpage_url")) && !pJSON->d[_T("webpage_url")].IsNull()) stream.webpage_url = pJSON->d[_T("webpage_url")].GetString();

        if (!s.sYDLSubsPreference.IsEmpty()) {
            if (pJSON->d.HasMember(_T("subtitles")) && !pJSON->d[_T("subtitles")].IsNull() && pJSON->d[_T("subtitles")].IsObject()) {
                loadSub(pJSON->d[_T("subtitles")], stream.subtitles);
            }
            if (s.bUseAutomaticCaptions) {
                if (pJSON->d.HasMember(_T("automatic_captions")) && !pJSON->d[_T("automatic_captions")].IsNull() && pJSON->d[_T("automatic_captions")].IsObject()) {
                    loadSub(pJSON->d[_T("automatic_captions")], stream.subtitles, true);
                }
            }
        }

        if (filterVideo(pJSON->d[_T("formats")], ydl_sd, s.iYDLMaxHeight, s.bYDLAudioOnly, s.iYDLVideoFormat)) {
            stream.video_url = ydl_sd.url;
            stream.audio_url = _T("");
            // find separate audio stream
            if (ydl_sd.has_video && !ydl_sd.has_audio) {
                if (filterAudio(pJSON->d[_T("formats")], ydl_sd)) {
                    if (ydl_sd.url != stream.video_url) {
                        stream.audio_url = ydl_sd.url;
                        #if YDL_TRACE
                        TRACE(_T("selected video url = %s\n"), static_cast<LPCWSTR>(stream.video_url));
                        TRACE(_T("selected audio url = %s\n"), static_cast<LPCWSTR>(stream.audio_url));
                        #endif
                    }
                }
            }
            streams.AddTail(stream);
        } else if (filterAudio(pJSON->d[_T("formats")], ydl_sd)) {
            stream.audio_url = ydl_sd.url;
            stream.video_url = _T("");
            streams.AddTail(stream);
        }
    } else {
        if (pJSON->d.HasMember(_T("id")) && !pJSON->d[_T("id")].IsNull()) info.id = pJSON->d[_T("id")].GetString();
        if (pJSON->d.HasMember(_T("title")) && !pJSON->d[_T("title")].IsNull()) info.title = pJSON->d[_T("title")].GetString();
        if (pJSON->d.HasMember(_T("uploader")) && !pJSON->d[_T("uploader")].IsNull()) info.uploader = pJSON->d[_T("uploader")].GetString();
        if (pJSON->d.HasMember(_T("uploader_id")) && !pJSON->d[_T("uploader_id")].IsNull()) info.uploader_id = pJSON->d[_T("uploader_id")].GetString();
        if (pJSON->d.HasMember(_T("uploader_url")) && !pJSON->d[_T("uploader_url")].IsNull()) info.uploader_url = pJSON->d[_T("uploader_url")].GetString();
        if (pJSON->d.HasMember(_T("entries"))) {
            YDL_LOG(_T("Parsing playlist"));
            const Value& entries = pJSON->d[_T("entries")];

            for (rapidjson::SizeType i = 0; i < entries.Size(); i++) {
                YDL_LOG(_T("Playlist entry %d"), i);
                const Value& entry = entries[i];

                if (entry.HasMember(_T("title")) && !entry[_T("title")].IsNull()) {
                    stream.title = entry[_T("title")].GetString();
                }

                if (!entry.HasMember(_T("formats")) || entry[_T("formats")].IsNull()) {
                    if (entry.HasMember(_T("url")) && !entry[_T("url")].IsNull()) {
                        stream.video_url = entry[_T("url")].GetString();
                        stream.audio_url = _T("");
                        if (!stream.video_url.IsEmpty()) {
                            streams.AddTail(stream);
                        }
                    }
                    continue;
                }

                if (filterVideo(entry[_T("formats")], ydl_sd, s.iYDLMaxHeight, s.bYDLAudioOnly, s.iYDLVideoFormat)) {
                    stream.video_url = ydl_sd.url;
                    stream.audio_url = _T("");
                    if (entry.HasMember(_T("series")) && !entry[_T("series")].IsNull()) stream.series = entry[_T("series")].GetString();
                    if (entry.HasMember(_T("season")) && !entry[_T("season")].IsNull()) stream.season = entry[_T("season")].GetString();
                    if (entry.HasMember(_T("season_number")) && !entry[_T("season_number")].IsNull()) stream.season_number = entry[_T("season_number")].GetInt();
                    if (entry.HasMember(_T("season_id")) && !entry[_T("season_id")].IsNull()) stream.season_id = entry[_T("season_id")].GetString();
                    if (entry.HasMember(_T("episode")) && !entry[_T("episode")].IsNull()) stream.episode = entry[_T("episode")].GetString();
                    if (entry.HasMember(_T("episode_number")) && !entry[_T("episode_number")].IsNull()) stream.episode_number = entry[_T("episode_number")].GetInt();
                    if (entry.HasMember(_T("episode_id")) && !entry[_T("episode_id")].IsNull()) stream.episode_id = entry[_T("episode_id")].GetString();
                    if (entry.HasMember(_T("webpage_url")) && !entry[_T("webpage_url")].IsNull()) stream.webpage_url = entry[_T("webpage_url")].GetString();
                    if (!s.sYDLSubsPreference.IsEmpty()) {
                        if (entry.HasMember(_T("subtitles")) && !entry[_T("subtitles")].IsNull() && entry[_T("subtitles")].IsObject()) {
                            loadSub(entry[_T("subtitles")], stream.subtitles);
                        }
                        if (s.bUseAutomaticCaptions && entry.HasMember(_T("automatic_captions")) && !entry[_T("automatic_captions")].IsNull() && entry[_T("automatic_captions")].IsObject()) {
                            loadSub(entry[_T("automatic_captions")], stream.subtitles);
                        }
                    }
                    if (ydl_sd.has_video && !ydl_sd.has_audio && entry.HasMember(_T("formats")) && !entry[_T("formats")].IsNull()) {
                        if (filterAudio(entry[_T("formats")], ydl_sd)) {
                            stream.audio_url = ydl_sd.url;
                        }
                    }
                    streams.AddTail(stream);
                } else if (filterAudio(entry[_T("formats")], ydl_sd)) {
                    stream.audio_url = ydl_sd.url;
                    stream.video_url = _T("");
                    streams.AddTail(stream);
                }
            }
        }
    }
    return !streams.IsEmpty();
}

bool CYoutubeDLInstance::loadJSON()
{
    if (!buf_out) {
        return false;
    }
    //the JSON buffer is ASCII with Unicode encoded with escape characters
    pJSON->d.Parse<rapidjson::kParseDefaultFlags, rapidjson::ASCII<>>(buf_out);
    if (pJSON->d.HasParseError()) {
        return false;
    }
    if (!pJSON->d.IsObject() || !pJSON->d.HasMember(_T("extractor"))) {
        return false;
    }
    bIsPlaylist = pJSON->d.FindMember(_T("entries")) != pJSON->d.MemberEnd();
    return true;
}

void CYoutubeDLInstance::loadSub(const Value& obj, CAtlList<YDLSubInfo>& subs, bool isAutomaticCaptions /*= false*/) {
    auto& s = AfxGetAppSettings();
    CAtlList<CString> preferlist;
    if (!s.sYDLSubsPreference.IsEmpty()) {
        if (s.sYDLSubsPreference.Find(_T(',')) != -1) {
            ExplodeMin(s.sYDLSubsPreference, preferlist, ',');
        } else {
            ExplodeMin(s.sYDLSubsPreference, preferlist, ' ');
        }
    }
    if (!isAutomaticCaptions) {
        subs.RemoveAll();
    }
    for (Value::ConstMemberIterator iter = obj.MemberBegin(); iter != obj.MemberEnd(); ++iter) {
        CString lang(iter->name.GetString());
        if (!preferlist.IsEmpty() && !isPrefer(preferlist, lang)) {
            continue;
        }
        if (iter->value.IsArray()) {
            const Value& arr = obj[(LPCTSTR)lang];
            for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
                const Value& dict = arr[i];
                YDLSubInfo sub;
                sub.isAutomaticCaptions = isAutomaticCaptions;
                sub.lang = lang;
                if (dict.HasMember(_T("ext")) && !dict[_T("ext")].IsNull()) {
                    sub.ext = dict[_T("ext")].GetString();
                }
                if (dict.HasMember(_T("url")) && !dict[_T("url")].IsNull()) {
                    sub.url = dict[_T("url")].GetString();
                }
                if (dict.HasMember(_T("data")) && !dict[_T("data")].IsNull() && dict[_T("data")].IsString()) {
                    sub.data = dict[_T("data")].GetString();
                }
                if (!sub.url.IsEmpty() || !sub.data.IsEmpty()) {
                    if (sub.ext.IsEmpty() || sub.ext == _T("vtt") || sub.ext == _T("ass") || sub.ext == _T("srt")) {
                        subs.AddTail(sub);
                    }
                }
            }
        }
    }
}

bool CYoutubeDLInstance::isPrefer(CAtlList<CString>& list, CString& lang) {
    POSITION pos = list.GetHeadPosition();
    while (pos) {
        CString la = list.GetNext(pos);
        if (lang.Left(la.GetLength()) == la) return true;
    }
    return false;
}
