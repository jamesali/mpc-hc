# Media Player Classic - Home Cinema

Media Player Classic - Home Cinema (MPC-HC) is a free and open-source video and audio player for Windows. MPC-HC is based on the original Guliverkli project and contains many additional features and bug fixes.

## Development Status

The player is mature, stable, reliable, and already provides a huge amount of features.

Due to a lack of active developers, the player is currently in maintenance mode. This means that there are no direct plans for adding any big new features. Development is currently limited to small bug fixes and updates. However, external developers can still contribute additional fixes and new functionality. In fact, several people have already helped to give the player some new functionality and other improvements.

The internal codecs are developed in an external project ([LAV Filters](https://github.com/Nevcairiel/LAVFilters)). That project is still actively maintained, and MPC-HC will be updated periodically with the latest codecs. Same applies to code/files from other external projects that are used by MPC-HC, such as MediaInfo.

If you are a programmer and want to help improve MPC-HC, then you can do so by submitting a pull request containing your patches.

## Overview of features

A lot of people seem to be unaware of some of the awesome features that have been added to MPC-HC in the past years. Here is a list of useful options and features that everyone should know about:
* Dark interface<br/>
Menu > View > Dark Theme<br/>
When using dark theme it is also possible to change the height of the seekbar and size of the toolbar buttons.<br/>
Options > Advanced
* Video preview on the seekbar<br/>
Options > Tweaks > Show preview on seek bar
* Adjust playback speed<br/>
Menu > Play > Playback rate<br/>
The buttons in the player that control playback rate take a 2x step by default. This can be customized to smaller values (like 10%):<br/>
Options > Playback > Speed step<br/>
Adjusting playback speed works best with the SaneAR audio renderer. This also has automatic pitch correction.<br/>
Options > Playback > Output > Audio Renderer
* MPC-HC can remember playback position, so you can resume from that point later<br/>
Options > Player > History
* You can quickly seek through a video with Ctrl + Mouse Scrollwheel.
* You can jump to next/previous file in a folder by pressing PageUp/PageDown.
* You can perform automatic actions at end of file. For example to go to next file or close player.<br/>
Options > Playback > After Playback (permanent setting)<br/>
Menu > Play > After Playback (for current file only)
* A-B repeat<br/>
You can loop a segment of a video. Press [ and ] to set start and stop markers.
* You can rotate/flip/mirror/stretch/zoom the video<br/>
Menu > View > Pan&Scan<br/>
This is also easily done with hotkeys (see below).
* There are lots of keyboard hotkeys and mouse actions to control the player. They can be customized as well.<br/>
Options > Player > Keys<br/>
Tip: there is a search box above the table.
* You can stream videos directly from Youtube and many other video websites<br/>
Put [yt-dlp.exe](https://github.com/yt-dlp/yt-dlp/releases) in the MPC-HC installation folder.<br/>
Then you can open website URLs in the player: Menu > File > Open File/URL<br/>
You can even download those videos: Menu > File > Save a copy<br/>
Tip: to be able to download in best quality with yt-dlp/youtube-dl, it is recommended to also put [ffmpeg.exe](https://www.gyan.dev/ffmpeg/builds/) in the MPC-HC folder.<br/>
Several YDL configuration options are found here: Options > Advanced<br/>
This includes an option to specify the location of the .exe in case you don't want to put it in MPC-HC folder.<br/>
Note: instead of yt-dlp.exe you can also use [youtube-dl.exe](https://youtube-dl.org/downloads/latest/youtube-dl.exe). I do however recommend using yt-dlp because that works better and has active development, while youtube-dl development is currently inactive.<br/>
Note 2: You also need to install [Microsoft Visual C++ 2010 SP1 Redistributable Package (x86)](https://download.microsoft.com/download/1/6/5/165255E7-1014-4D0A-B094-B6A430A6BFFC/vcredist_x86.exe)
* Play HDR video<br/>
This requires using [madVR](http://forum.doom9.org/showthread.php?t=146228) or [MPC Video Renderer](https://github.com/Aleksoid1978/VideoRenderer/releases).<br/>
After installation these renderers can be selected here:<br/>
Options > Playback > Output
* Ability to search for and download subtitles, either automatically or manually (press D):<br/>
Options > Subtitles > Misc
* Besides all these (new) features, there have also been many bugfixes and internal improvements in the player in the past years that give better performance and stability. It also has updated internal codecs. Support was added for CUE sheets, WebVTT subtitles, etc.

## System Requirements

* Windows 7 / 8 / 8.1 / 10 / 11

## License

MPC-HC's code is licensed under [GPL v3](/COPYING.txt).
