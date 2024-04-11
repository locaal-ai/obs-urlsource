# URL/API Source - OBS Plugin

<div align="center">

[![GitHub](https://img.shields.io/github/license/occ-ai/obs-urlsource)](https://github.com/occ-ai/obs-urlsource/blob/main/LICENSE)
[![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/occ-ai/obs-urlsource/push.yaml)](https://github.com/occ-ai/obs-urlsource/actions/workflows/push.yaml)
[![Total downloads](https://img.shields.io/github/downloads/occ-ai/obs-urlsource/total)](https://github.com/occ-ai/obs-urlsource/releases)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/occ-ai/obs-urlsource)](https://github.com/occ-ai/obs-urlsource/releases)
[![Discord](https://img.shields.io/discord/1200229425141252116)](https://discord.gg/KbjGU2vvUz)

</div>

## Introduction

The URL/API Source is a plugin for [OBS Studio](https://obsproject.com) that allows users to add a media source that fetches data from a URL or API endpoint and displays it as text. [OBS Forums page](https://obsproject.com/forum/resources/url-api-source-fetch-live-data-and-display-it-on-screen.1756/) | [Blog post](https://www.morethantechnical.com/2023/08/10/url-api-source-obs-plugin/)

If this free plugin has been valuable to you consider adding a ⭐ to this GH repo, rating it [on OBS](https://obsproject.com/forum/resources/url-api-source-fetch-live-data-and-display-it-on-screen.1756/), subscribing to [my YouTube channel](https://www.youtube.com/@royshilk) where I post updates, and supporting my work: https://github.com/sponsors/royshil

### Usage Tutorials
Watch quick tutorials on how to use and setup the URL/API source on your OBS scene.
<div align="center">
  <a href="https://youtu.be/E_UMNIfgR5w" target="_blank">
    <img width="27%" src="https://github-production-user-asset-6210df.s3.amazonaws.com/441170/258666347-327a632f-62f3-4365-af8e-6bb91f5a56ef.jpeg" />
  </a>
  <a href="https://youtu.be/hwHgNcPJEfM" target="_blank">
    <img width="27%" src="https://github-production-user-asset-6210df.s3.amazonaws.com/441170/271332973-a482c56a-6c21-494b-b8a4-95216210ab58.jpeg" />
  </a>
  <a href="https://youtu.be/kgAOCijJ51Q" target="_blank">
    <img width="27%" src="https://github-production-user-asset-6210df.s3.amazonaws.com/441170/280917569-fba369e7-b91f-4e76-90ff-09e8cbb75ffc.jpeg" />
  </a>
  <br/>
  https://youtu.be/E_UMNIfgR5w &amp; https://youtu.be/hwHgNcPJEfM &amp; <a href="https://youtu.be/kgAOCijJ51Q" target="_blank">HTML Scraping Tutorial (18 min)</a>
</div>

#### AI on OBS Tutorials
Bring AI to your OBS! Be a x10 streamer and content creator with AI tools:
<div align="center">
  <a href="https://youtu.be/4BTmoKr0YMw" target="_blank">
    <img width="27%" src="https://github-production-user-asset-6210df.s3.amazonaws.com/441170/283315931-70c0c583-d1dc-4bd6-9ace-86c8e47f1229.jpg" />
  </a>
  <a href="https://youtu.be/2wJ72DcgBew" target="_blank">
    <img width="27%" src="https://github-production-user-asset-6210df.s3.amazonaws.com/441170/284642194-6c97a6e7-3ba3-4e57-b0b6-612615266ae6.jpeg" />
  </a>
  <a href="https://youtu.be/kltJbg9hH4s" target="_blank">
    <img width="27%" src="https://github-production-user-asset-6210df.s3.amazonaws.com/441170/284643465-a7aa2d13-c968-404d-8300-827fe069832d.jpg" />
  </a>
</div>

#### Inspiration
Out of ideas on what to do with URL/API Source? Here's a repo with 1,000s of public APIs https://github.com/public-apis/public-apis

#### Dynamic Templating
The URL source supports both input and output templating using the [Inja](https://github.com/pantor/inja) engine.

Output templates include `{{output}}` in case of a singular extraction from the response or `{{output1}},{{output2}},...` in case of multiple extracted value. In addition the `{{body}}` variable contains the entire body of the response in case of JSON. Advanced output templating functions can be achieved through [Inja](https://github.com/pantor/inja) like looping over arrays, etc.

<div align="center">
<img width="50%" src="https://github.com/occ-ai/obs-urlsource/assets/441170/2b7a4ceb-3c38-4afd-82b3-675c0fa8c5fe" />
</div>

The input template works for the URL (querystring or REST path) or the POST body

<div align="center">
<img width="50%" src="https://github.com/occ-ai/obs-urlsource/assets/441170/ae6b9e04-ff5a-441b-a94c-427b1e7c76b3" />
</div>

Use the `{{input}}` variable to insert the output from a Text source. Inja advanced templates are available too.
A special function `strftime` is available for formatting the current time using conventions from C++ STL ([strftime](https://en.cppreference.com/w/cpp/chrono/c/strftime)), as well as `urlencode` which is useful for dynamic input in the querystring.

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
- Dynamic input from a Text or Image source (base64)
- Output parsing: JSON via JSONPointer & JSONPath, XML/HTML via XPath & XQuery, Regex and CSS selectors
- Update timer for live updating data
- Test of the request to find the right parsing
- Output styling (font, color, etc.) and formatting (via regex post processing)
- Output Image (via image URL or image data on the response)
- Output text to external Text Source
- Output audio to external Media Source
- Output to multiple sources with one request (Output Mapping)
- Multi-value (array, union) parsed output capture, object unpacking (via Inja)
- Dynamic input aggregations (time-based, "empty"-based)

Coming soon:
- Authentication (Basic, Digest, OAuth)
- Websocket support
- More parsing options (CSV, etc.)
- More request types (HTTP PUT / DELETE / PATCH, and GraphQL)
- More output formats (Markdown, slim, reStructured, HAML, etc.)

Check out our other plugins:
- [Background Removal](https://github.com/occ-ai/obs-backgroundremoval) removes background from webcam without a green screen.
- [Detect](https://github.com/occ-ai/obs-detect) will find and track >80 types of objects in any source that provides an image in real-time
- [LocalVocal](https://github.com/occ-ai/obs-localvocal) speech AI assistant plugin for real-time, local transcription (captions), translation and more language functions
- [Polyglot](https://github.com/occ-ai/obs-polyglot) translation AI plugin for real-time, local translation to hunderds of languages
- 🚧 Experimental 🚧 [CleanStream](https://github.com/occ-ai/obs-cleanstream) for real-time filler word (uh,um) and profanity removal from live audio stream

If you like this work, which is given to you completely free of charge, please consider supporting it on GitHub: https://github.com/sponsors/royshil

## Download
Check out the [latest releases](https://github.com/occ-ai/obs-urlsource/releases) for downloads and install instructions.


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
