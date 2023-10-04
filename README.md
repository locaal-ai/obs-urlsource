# URL/API Source - OBS Plugin

<div align="center">

[![GitHub](https://img.shields.io/github/license/royshil/obs-urlsource)](https://github.com/royshil/obs-urlsource/blob/main/LICENSE)
[![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/royshil/obs-urlsource/push.yaml)](https://github.com/royshil/obs-urlsource/actions/workflows/push.yaml)
[![Total downloads](https://img.shields.io/github/downloads/royshil/obs-urlsource/total)](https://github.com/royshil/obs-urlsource/releases)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/royshil/obs-urlsource)](https://github.com/royshil/obs-urlsource/releases)

</div>

## Introduction

The URL/API Source is a plugin for [OBS Studio](https://obsproject.com) that allows users to add a media source that fetches data from a URL or API endpoint and displays it as text. [OBS Forums page](https://obsproject.com/forum/resources/url-api-source-fetch-live-data-and-display-it-on-screen.1756/) | [Blog post](https://www.morethantechnical.com/2023/08/10/url-api-source-obs-plugin/)

If this free plugin has been valuable to you consider adding a ⭐ to this GH repo, rating it [on OBS](https://obsproject.com/forum/resources/url-api-source-fetch-live-data-and-display-it-on-screen.1756/), subscribing to [my YouTube channel](https://www.youtube.com/@royshilk) where I post updates, and supporting my work: https://github.com/sponsors/royshil

### Usage Tutorial
Watch a short tutorial on how to use and setup the URL/API source on your OBS scene.
<div align="center">
  <a href="https://youtu.be/E_UMNIfgR5w" target="_blank">
    <img width="40%" src="https://github-production-user-asset-6210df.s3.amazonaws.com/441170/258666347-327a632f-62f3-4365-af8e-6bb91f5a56ef.jpeg" />
  </a>
  <a href="https://youtu.be/hwHgNcPJEfM" target="_blank">
    <img width="40%" src="https://github-production-user-asset-6210df.s3.amazonaws.com/441170/271332973-a482c56a-6c21-494b-b8a4-95216210ab58.jpeg" />
  </a>
  <br/>
  https://youtu.be/E_UMNIfgR5w &amp; https://youtu.be/hwHgNcPJEfM
</div>

### Code Walkthrough
Watch an explanation of the major parts of the code and how they work together.
<div align="center">
  <a href="https://youtu.be/TiluUg1LxcQ" target="_blank">
    <img width="50%" src="https://github-production-user-asset-6210df.s3.amazonaws.com/441170/258929032-08f74e90-0260-41db-8674-94bd630855f8.jpeg" />
  </a><br/>
  https://youtu.be/TiluUg1LxcQ
</div>

Features:
- HTTP request types: GET, POST
- Request headers (for e.g. API Key or Auth token)
- Request body for POST
- Output parsing: JSON via JSONPointer, XML/HTML via XPath and Regex
- Update timer for live streaming data
- Test of the request to find the right parsing
- Output styling (font, color, etc.) and formatting (via regex post processing)
- Image output (via URL)
- Output to external Text Source

Coming soon:
- Authentication (Basic, Digest, OAuth)
- Websocket support
- Multi-value parsed output capture and advanced templating
- More output parsing options (JSONPath, CSS selectors, etc.)
- More request types (HTTP PUT / DELETE / PATCH, and GraphQL)
- More output formats (XML, HTML, CSV, etc.)
- More output types (Video, Audio, etc.)
- Output to OBS sources (Image, etc.)

Check out our other plugins:
- [Background Removal](https://github.com/royshil/obs-backgroundremoval) removes background from webcam without a green screen.
- 🚧 Experimental 🚧 [CleanStream](https://github.com/royshil/obs-cleanstream) for real-time filler word (uh,um) and profanity removal from live audio stream
- [LocalVocal](https://github.com/royshil/obs-localvocal) speech AI assistant plugin for real-time transcription (captions), translation and more language functions

If you like this work, which is given to you completely free of charge, please consider supporting it on GitHub: https://github.com/sponsors/royshil

## Download
Check out the [latest releases](https://github.com/royshil/obs-urlsource/releases) for downloads and install instructions.


## Building

The plugin was built and tested on Mac OSX  (Intel & Apple silicon), Windows and Linux.

Start by cloning this repo to a directory of your choice.

### Mac OSX

Using the CI pipeline scripts, locally you would just call the zsh script. By default this builds a universal binary for both Intel and Apple Silicon. To build for a specific architecture please see `.github/scripts/.build.zsh` for the `-arch` options.

```sh
$ ./.github/scripts/build-macos -c Release
```

#### Install
The above script should succeed and the plugin files (e.g. `obs-urlsource.plugin`) will reside in the `./release/Release` folder off of the root. Copy the `.plugin` file to the OBS directory e.g. `~/Library/Application Support/obs-studio/plugins`.

To get `.pkg` installer file, run for example
```sh
$ ./.github/scripts/package-macos -c Release
```
(Note that maybe the outputs will be in the `Release` folder and not the `install` folder like `pakage-macos` expects, so you will need to rename the folder from `build_x86_64/Release` to `build_x86_64/install`)

### Linux (Ubuntu)

Use the CI scripts again
```sh
$ ./.github/scripts/build-linux.sh
```

Copy the results to the standard OBS folders on Ubuntu
```sh
$ sudo cp -R release/RelWithDebInfo/lib/* /usr/lib/x86_64-linux-gnu/
$ sudo cp -R release/RelWithDebInfo/share/* /usr/share/
```
Note: The official [OBS plugins guide](https://obsproject.com/kb/plugins-guide) recommends adding plugins to the `~/.config/obs-studio/plugins` folder.

### Windows

Use the CI scripts again, for example:

```powershell
> .github/scripts/Build-Windows.ps1 -Target x64 -CMakeGenerator "Visual Studio 17 2022"
```

The build should exist in the `./release` folder off the root. You can manually install the files in the OBS directory.
