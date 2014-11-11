harcurl
=======

HTTP Archive (HAR) command-line tool


Introduction
------------

The `curl` tool has many options, but it can be difficult to use. HAR is a great format
for debugging and logging, but it's very static, and just sits there. This is where
`harcurl` comes in. To make a request, simply fill out a HAR `entry` with the
`request` fields you are expecting to send, and `harcurl` will:

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

* `libcurl` for obvious reasons (all HTTP logic is handled by it)
* `glib-2.0` for byte-array utils
* `jansson` for JSON parsing, reading and writing
* `zlib` for GZIP compression utils


Examples
--------

<pre>
$ cat > req.json <<EOF
{
    "request": {
        "method": "GET",
        "url": "http://httpbin.org/get"
    }
}
EOF

$ harcurl < req.json > resp.json
</pre>
