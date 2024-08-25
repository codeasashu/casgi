## Process Flow

### 1. Initializing python application

This is done in master process. 
See [this](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/uwsgi.c#L2169) how application dict is initialized.
Master process uses `-w` parameter to get the python module name to find `applications`. If it couldn't, it tries to find `application`
callable. If it does find `application`, it maps it to `/` path. Hence, the `uwsgi->py_apps` looks likes `{'/':application}`.

### 2. Listening and binding on socket
The master then continues to listen and bind on a socket. [See the code](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/uwsgi.c#L783)
uwsgi protocol uses [modifiers](https://uwsgi-docs.readthedocs.io/en/latest/Protocol.html). There can be many modifiers. Our interest is modifier `0`, which is to handle
HTTP request. The uwsgi initializes who will handle the request on each modifiers. It maps these modifiers to handlers in `hooks`.

```c
uwsgi.shared->hooks[0] = uwsgi_request_wsgi
```
Eg: WSGI request handler is [mapped to hook 0](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/uwsgi.c#L889)

### 3. Create workers and accept requests
The master then creates workers based on provided params. [See code](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/uwsgi.c#L1026)
Inside each worker, it accepts on the server fd (the socket on which master is listening) [See code](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/uwsgi.c#L1557). NOTE: this is blocking until next request is received.

### 4. Handling the requests
Once the request is received in worker `wsgi_req_recv` function [See code](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/utils.c#L319),
the header is parsed from the packet [See code](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/protocol.c#L203).
The packet header looks like:
```c
struct uwsgi_packet_header {
    uint8_t modifier1;
    uint16_t datasize;
    uint8_t modifier2;
};
```
the body is read based on size of `datasize`. This completes the preperation of `wsgi_req`. 
The next line in the worker receiver `wsgi_req_recv` function does the magic:
    1. It gets the modifier from `wsgi_req` (which will be `0` for HTTP requests)
    2. It finds the handler in `hooks` dict.
    3. It invokes the handler. The code `wsgi_req->async_status = (*uwsgi.shared->hooks[wsgi_req->uh.modifier1]) (&uwsgi, wsgi_req);` does the magic.
    4. It sends the response back to the client.

```c
// In this, 
// wsgi_req->uh.modifier1 = 0
// uwsgi.shared->hooks = uwsgi_request_wsgi
wsgi_req->async_status = (*uwsgi.shared->hooks[wsgi_req->uh.modifier1]) (&uwsgi, wsgi_req);
// means
wsgi_req->async_status = uwsgi_request_wsgi(&uwsgi, wsgi_req);
```

### 5. Invoking python application
The `uwsgi_request_wsgi` is defined [here](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/wsgi_handlers.c#L73)

The `uwsgi_parse_vars` [function](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/protocol.c#L293) is used inside `uwsgi_request_wsgi` [here](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/wsgi_handlers.c#L105) to parse the `wsgi_req` struct based on HTTP headers.
For instance, the `wsgi_req->script_name` is set to HTTP request path (eg: `/foo`). 
It then tries to find the handler for this in the `uwsgi->py_apps` (See section 1 - Initializing python application).

1. `uwsgi->py_apps` contains mappings of app routes (str) -> app ids (handler ids, keys to `wsgi_apps` dict) `{'/':application, '/django':'application', '/myapp':myapp}`.
2. When a request comes, it goes to `uwsgi_request_wsgi` function [see](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/wsgi_handlers.c#L73)
3. It tries to get script name from uwsgi request (`/app`) and get the app_id from `uwsgi->py_apps`
4. After getting `app_id` above, it gets `wsgi_app` from `uwsgi->wsgi_apps` dict.
5. Set some vars, and call `python_call(wi->wsgi_callable, wsgi_req->async_args)`. Each `wsgi_apps` will have a python callable.
6. Incase no applications are found, the log is:
```
server side:
[pid: 18618|app: -1|req: -1/1] 127.0.0.1 () {32 vars in 352 bytes} [Thu Aug 22 02:04:01 2024] GET / => generated 46 bytes in 0 msecs (HTTP/1.1 500) 2 headers in 63 bytes (0 async switches on async core 0)

caller side:
> GET / HTTP/1.1
> User-Agent: curl/7.29.0
> Host: localhost5:8080
> Accept: */*
>
< HTTP/1.1 500 Internal Server Error
< Server: nginx/1.20.1
< Date: Wed, 21 Aug 2024 20:34:49 GMT
< Content-Type: text/html
< Transfer-Encoding: chunked
< Connection: keep-alive
<h1>uWSGI Error</h1>wsgi application not found
```


## Dynamic Apps
You have a python file:
```py
import uwsgi
import django.core.handlers.wsgi

application = django.core.handlers.wsgi.WSGIHandler()

def myapp(environ, start_response):
        start_response('200 OK', [('Content-Type', 'text/plain')])
        yield 'Hello World\n'



uwsgi.applications = {'/':application, '/django':'application', '/myapp':myapp}
```
Passing the python module name (without the .py extension) to the -w option of uWSGI, it will search the uwsgi.applications dictionary for the url/callable mappings.
The value of every item can be the function/callable string representation or the object itself.

Releavent code: 

1. See how `&uwsgi->wsgi_apps` collects all the available applications in the module.
2. `uwsgi->py_apps` contains mappings of app routes (str) -> app ids (handler ids, keys to `wsgi_apps` dict) `{'/':application, '/django':'application', '/myapp':myapp}`.
3. When a request comes, it goes to `uwsgi_request_wsgi` function [see](https://github.com/unbit/uwsgi/blob/79fa25c4e03b38b5850f9134b4ee873eed3ae8b9/wsgi_handlers.c#L73)
4. It tries to get script name from uwsgi request (`/app`) and get the app_id from `uwsgi->py_apps`
5. After getting `app_id` above, it gets `wsgi_app` from `uwsgi->wsgi_apps` dict.
6. Set some vars, and call `python_call(wi->wsgi_callable, wsgi_req->async_args)`. Each `wsgi_apps` will have a python callable.
7. Incase no applications are found, the log is:
```
server side:
[pid: 18618|app: -1|req: -1/1] 127.0.0.1 () {32 vars in 352 bytes} [Thu Aug 22 02:04:01 2024] GET / => generated 46 bytes in 0 msecs (HTTP/1.1 500) 2 headers in 63 bytes (0 async switches on async core 0)

caller side:
> GET / HTTP/1.1
> User-Agent: curl/7.29.0
> Host: localhost5:8080
> Accept: */*
>
< HTTP/1.1 500 Internal Server Error
< Server: nginx/1.20.1
< Date: Wed, 21 Aug 2024 20:34:49 GMT
< Content-Type: text/html
< Transfer-Encoding: chunked
< Connection: keep-alive
<h1>uWSGI Error</h1>wsgi application not found
```



---
### Runs

Running with master with 2 workers and no python app
```sh
root@s4 uwsgi-0.9.5]# ./uwsgi -s 127.0.0.1:3031 -p 2 -M
*** Starting uWSGI 0.9.5-dev (64bit) on [Thu Aug 22 01:49:56 2024] ***
*** Warning Python3.x support is experimental, do not use it in production environment ***
Python version: 3.6.7 (default, Dec  5 2018, 15:02:05)
[GCC 4.8.5 20150623 (Red Hat 4.8.5-36)]
uWSGI running as root, you can use --uid/--gid/--chroot options
 *** WARNING: you are running uWSGI as root !!! (use the --uid flag) ***
your memory page size is 4096 bytes
allocated 536 bytes (0 KB) for 1 request\'s buffer.
binding on TCP port: 3031
your server socket listen backlog is limited to 64 connections
initializing hooks...done.
*** uWSGI is running in multiple interpreter mode !!! ***
spawned uWSGI master process (pid: 32761)
spawned uWSGI worker 1 (pid: 32762)
spawned uWSGI worker 2 (pid: 32763)
```

Running without master
```sh
[root@s4 uwsgi-0.9.5]# ./uwsgi -s 127.0.0.1:3031
*** Starting uWSGI 0.9.5-dev (64bit) on [Thu Aug 22 22:40:45 2024] ***
*** Warning Python3.x support is experimental, do not use it in production environment ***
Python version: 3.6.7 (default, Dec  5 2018, 15:02:05)
[GCC 4.8.5 20150623 (Red Hat 4.8.5-36)]
uWSGI running as root, you can use --uid/--gid/--chroot options
 *** WARNING: you are running uWSGI as root !!! (use the --uid flag) ***
 *** WARNING: you are running uWSGI without its master process manager ***
your memory page size is 4096 bytes
allocated 536 bytes (0 KB) for 1 request\'s buffer.
binding on TCP port: 3031
your server socket listen backlog is limited to 64 connections
initializing hooks...done.
*** uWSGI is running in multiple interpreter mode !!! ***
spawned uWSGI worker 1 (and the only) (pid: 8437)
```

Running with master but no worker:
```sh
[root@s4 uwsgi-0.9.5]./uwsgi -s 127.0.0.1:3031 -M
*** Starting uWSGI 0.9.5-dev (64bit) on [Thu Aug 22 22:41:31 2024] ***
*** Warning Python3.x support is experimental, do not use it in production environment ***
Python version: 3.6.7 (default, Dec  5 2018, 15:02:05)
[GCC 4.8.5 20150623 (Red Hat 4.8.5-36)]
uWSGI running as root, you can use --uid/--gid/--chroot options
 *** WARNING: you are running uWSGI as root !!! (use the --uid flag) ***
your memory page size is 4096 bytes
allocated 536 bytes (0 KB) for 1 request\'s buffer.
binding on TCP port: 3031
your server socket listen backlog is limited to 64 connections
initializing hooks...done.
*** uWSGI is running in multiple interpreter mode !!! ***
spawned uWSGI master process (pid: 9416)
spawned uWSGI worker 1 (pid: 9417)
^CSIGINT/SIGQUIT received...killing workers...
goodbye to uWSGI.
```

Running with python application

```sh
[root@s4 uwsgi-0.9.5]# ./uwsgi -s 127.0.0.1:3031 -p 2 -M -w myapp
*** Starting uWSGI 0.9.5-dev (64bit) on [Thu Aug 22 23:53:53 2024] ***
*** Warning Python3.x support is experimental, do not use it in production environment ***
Python version: 3.6.7 (default, Dec  5 2018, 15:02:05)
[GCC 4.8.5 20150623 (Red Hat 4.8.5-36)]
uWSGI running as root, you can use --uid/--gid/--chroot options
 *** WARNING: you are running uWSGI as root !!! (use the --uid flag) ***
your memory page size is 4096 bytes
allocated 536 bytes (0 KB) for 1 request\'s buffer.
binding on TCP port: 3031
your server socket listen backlog is limited to 64 connections
initializing hooks...done.
...getting the applications list from the 'myapp' module...
uwsgi.applications dictionary is not defined, trying with the "applications" one...
applications dictionary is not defined, trying with the "application" callable.
initializing [/] app...
application 0 (/) ready
setting default application to 0
spawned uWSGI master process (pid: 3874)
spawned uWSGI worker 1 (pid: 3875)
spawned uWSGI worker 2 (pid: 3876)
```

Running with python application and single workers
```sh
[root@s4 uwsgi-0.9.5]# ./uwsgi -s 127.0.0.1:3031 -w myapp
*** Starting uWSGI 0.9.5-dev (64bit) on [Thu Aug 22 23:55:00 2024] ***
*** Warning Python3.x support is experimental, do not use it in production environment ***
Python version: 3.6.7 (default, Dec  5 2018, 15:02:05)
[GCC 4.8.5 20150623 (Red Hat 4.8.5-36)]
uWSGI running as root, you can use --uid/--gid/--chroot options
 *** WARNING: you are running uWSGI as root !!! (use the --uid flag) ***
 *** WARNING: you are running uWSGI without its master process manager ***
your memory page size is 4096 bytes
allocated 536 bytes (0 KB) for 1 request\'s buffer.
binding on TCP port: 3031
your server socket listen backlog is limited to 64 connections
initializing hooks...done.
...getting the applications list from the 'myapp' module...
uwsgi.applications dictionary is not defined, trying with the "applications" one...
applications dictionary is not defined, trying with the "application" callable.
initializing [/] app...
application 0 (/) ready
setting default application to 0
spawned uWSGI worker 1 (and the only) (pid: 5214)
```

Running with invalid python application
```sh
[root@s4 uwsgi-0.9.5]# ./uwsgi -s 127.0.0.1:3031 -w myappm
*** Starting uWSGI 0.9.5-dev (64bit) on [Thu Aug 22 23:55:51 2024] ***
*** Warning Python3.x support is experimental, do not use it in production environment ***
Python version: 3.6.7 (default, Dec  5 2018, 15:02:05)
[GCC 4.8.5 20150623 (Red Hat 4.8.5-36)]
uWSGI running as root, you can use --uid/--gid/--chroot options
 *** WARNING: you are running uWSGI as root !!! (use the --uid flag) ***
 *** WARNING: you are running uWSGI without its master process manager ***
your memory page size is 4096 bytes
allocated 536 bytes (0 KB) for 1 request\'s buffer.
binding on TCP port: 3031
your server socket listen backlog is limited to 64 connections
initializing hooks...done.
ModuleNotFoundError: No module named 'myappm'
```

### Open items
1. Check when does `single_interpreter` is 1 or 0?
2. How do uwsgi manages multiple interpreters? Does it add one each time an application is found/added?
3. See uwsgi->py_apps


