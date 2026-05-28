Socket server and client snippet
================================

This snippet allows to create some utilities than can be run both in client or server mode.
The sockets are **local** TCP or UNIX sockets. Server-side polling use `poll()`.

The pieces of user code may be in comments marked `USERCODE`