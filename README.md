harcurl
=======

HTTP Archive (HAR) command-line tool


Introduction
------------

The `curl` tool has many options, but it can be difficult to use. HAR is a great format
for debugging and logging, but it's very static, and just sits there. This is where
`harcurl` comes in. To make a request, simply fill out a HAR `entry` with the
`request` fields you are sending, and `harcurl` will:

* read the JSON object from `stdin`
* convert it to `libcurl` options
* call curl_easy_setopt so that `libcurl` can use them
* call curl_easy_perform
* call curl_easy_getinfo to fill in the timings
* convert the response back to JSON
* dumps the JSON object to `stdout`

It is a very simply idea, but hopefully one that helps people to
understand, inspect, and use: CURL, HAR, and HTTP.


Dependancies
------------

* `libcurl` for obvious reasons (all HTTP logic is handled by it).
* `jansson` for JSON parsing, reading and writing.
* `glib` for byte-array utils, utf8 validation, etc.
* `zlib` for GZIP compression utils.


Examples
--------

<pre>
$ cat &gt; req.json &lt;&lt;EOF
{
    "request": {
        "method": "GET",
        "url": "http://httpbin.org/get"
    }
}
EOF

$ harcurl &lt; req.json &gt; resp.json
</pre>

HAR Extensions
--------------

HAR-1.2 is a great specification. It does miss a couple things, however, so harcurl uses
a few extensions to it where appropriate.

* `entry.request._headersText`
* `entry.request._requestLine`
  `entry.request._requestLine` should be the same as `{method} {_urlParts.path} {httpVersion}`
* `entry.request._urlParts`
  `entry.request.url` should be the same as `{_urlParts.scheme}://{_urlParts.authority}{_urlParts.path}`
* `entry.response._headersText`
* `entry.response._contentType`
* `entry.response._statusLine`
  `entry.response._statusLine` should be the same as `{httpVersion} {status} {statusText}`
