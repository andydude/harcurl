/* -*- mode: c; c-basic-offset: 2; tab-width: 80; -*- */
/* harcurl - HTTP Archive (HAR) support for libcurl
 * Copyright (C) 2014-2015  Andrew Robbins
 *
 * This library ("it") is free software; it is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; you can redistribute it and/or modify it under the terms of the
 * GNU Lesser General Public License ("LGPLv3") <https://www.gnu.org/licenses/lgpl.html>.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <glib.h>
#include <jansson.h>
#include <zlib.h>

#include "config.h"

gboolean global_verbose = FALSE;

/*
 * HarStatusCode:
 * 
 * This enumeration is designed to work with
 * libcurl status codes. The maximum status code
 * that libcurl uses at the time of this writing
 * is 89, so 128 should be enough for future
 * expansions, if libcurl wants to do so.
 */
typedef enum _HarStatusCode {
  HAR_OK = 0,
  
  HAR_ERROR_UNKNOWN = 0x80,   /* 128 */
  HAR_ERROR_NO_REQUEST,       /* 129 */
  HAR_ERROR_NO_RESPONSE,      /* 130 */
  HAR_ERROR_NO_METHOD,        /* 131 */
  HAR_ERROR_NO_URL,           /* 132 */
  HAR_ERROR_TEXT_AND_PARAMS,  /* 133 */
  HAR_ERROR_WITH_CURL,        /* 134 = libcurl returned an error */
  HAR_ERROR_WITH_HTTP,        /* 135 = HTTP protocol violation */
  HAR_ERROR_WITH_JANSSON,     /* 136 = libjansson returned an error */
  HAR_ERROR_WITH_JSON,        /* 137 = JSON was unparsable */
  
  HAR_ERROR_LAST,             /* 138 */
} HarStatusCode;

int
har_curl_formadd_strerror(int errnum, char * strerrbuf, size_t buflen)
{
  switch (errnum) {
  case CURL_FORMADD_MEMORY:
    strncpy(strerrbuf, "memory", buflen);
    break;
  case CURL_FORMADD_OPTION_TWICE:
    strncpy(strerrbuf, "option twice", buflen);
    break;
  case CURL_FORMADD_NULL:
    strncpy(strerrbuf, "null", buflen);
    break;
  case CURL_FORMADD_UNKNOWN_OPTION:
    strncpy(strerrbuf, "unknown option", buflen);
    break;
  case CURL_FORMADD_INCOMPLETE:
    strncpy(strerrbuf, "incomplete", buflen);
    break;
  case CURL_FORMADD_ILLEGAL_ARRAY:
    strncpy(strerrbuf, "illegal array", buflen);
    break;
  case CURL_FORMADD_DISABLED:
    strncpy(strerrbuf, "disabled", buflen);
    break;
  }

  return 0;
}

int
har_zlib_strerror(int errnum, char * strerrbuf, size_t buflen)
{
  switch (errnum) {
  case Z_STREAM_END:
    strncpy(strerrbuf, "stream end", buflen);
    break;
  case Z_NEED_DICT:
    strncpy(strerrbuf, "need dict", buflen);
    break;
  case Z_ERRNO:
    strncpy(strerrbuf, "error number", buflen);
    break;
  case Z_STREAM_ERROR:
    strncpy(strerrbuf, "stream error", buflen);
    break;
  case Z_DATA_ERROR:
    strncpy(strerrbuf, "data error", buflen);
    break;
  case Z_MEM_ERROR:
    strncpy(strerrbuf, "memory error", buflen);
    break;
  case Z_BUF_ERROR:
    strncpy(strerrbuf, "buf error", buflen);
    break;
  default:
    strncpy(strerrbuf, "unknown error", buflen);
    break;
  }

  return 0;
}

struct curl_slist *
har_headers_to_curl_slist(json_t * headers)
{
  struct curl_slist * result = NULL;
  json_t * vj;
  json_t * pj;
  const char * ks;
  const char * vs;
  int ix;
  
  json_array_foreach(headers, ix, pj) {
    ks = json_string_value(json_object_get(pj, "name"));
    vs = json_string_value(json_object_get(pj, "value"));
    size_t s_len = strlen(ks) + strlen(vs) + 5;
    char * s = g_malloc0(s_len);
    snprintf(s, s_len, "%s: %s", ks, vs);
    result = curl_slist_append(result, s);
  }
  
  return result;
}

void
har_headers_from_text(json_t * headers, const char * s, size_t s_len)
{
  int ix;
  gchar ** parts;
  gchar * line;
  gchar ** lines;
  json_t * header;
  const char * name;
  const char * value;
  if (!s) {
    fprintf(stderr, "har_headers_from_text(NULL)\n");
    return;
  }

  lines = g_strsplit(s, "\r\n", 100);
  for (ix = 0; lines[ix]; ++ix) {
    line = lines[ix];
    if (!line) break;
    if (strlen(line) == 0) continue;

    parts = g_strsplit(line, ":", 2);
    if (!parts) continue;
    if (!parts[1]) continue;
    header = json_object();
    name = parts[0];
    value = parts[1];

    /* to account for the space */
    if (value[0] == ' ') 
      value++;

    json_object_set_new(header, "name", json_string(name));
    json_object_set_new(header, "value", json_string(value));
    json_array_append_new(headers, header);
  }
}

int har_strerror(int status, char *strerrbuf, size_t buflen)
{
  switch (status) {
  case 0:
    strncpy("OK", strerrbuf, buflen);
    break;
  case HAR_ERROR_NO_REQUEST:
    strncpy("The request is missing", strerrbuf, buflen);
    break;
  case HAR_ERROR_NO_RESPONSE:
    strncpy("The response is missing", strerrbuf, buflen);
    break;
  case HAR_ERROR_NO_METHOD:
    strncpy("The method is missing. If you really want libcurl to automatically choose the method for you, then set the method to \"AUTO\"", strerrbuf, buflen);
    break;
  case HAR_ERROR_NO_URL:
    strncpy("The url property is missing, or was impossible to reconstruct with the information given.", strerrbuf, buflen);
    break;
  case HAR_ERROR_TEXT_AND_PARAMS:
    strncpy("Both text and params were given in the request.postData property. Please use one or the other, but not both.", strerrbuf, buflen);
    break;
  default:
    strncpy("unknown error", strerrbuf, buflen);
    return -1;
  }

  return 0;
}

struct curl_slist *
har_request_to_curl_slist(json_t * req)
{
  int ix;
  json_t * header;
  json_t * headers = json_object_get(req, "headers");
  
  json_array_foreach(headers, ix, header) {
    const char * name = json_string_value(json_object_get(header, "name"));
    
    if (!g_ascii_strcasecmp(name, "content-encoding")) {
      const char * value = json_string_value(json_object_get(header, "value"));
      json_object_set_new(req, "_contentEncoding", json_string(value));
    }

    if (!g_ascii_strcasecmp(name, "content-type")) {
      const char * value = json_string_value(json_object_get(header, "value"));
      json_object_set_new(req, "_contentType", json_string(value));
    }
  }
  
  return har_headers_to_curl_slist(headers);
}

struct curl_httppost *
har_request_postdata_to_curl_httppost(json_t * req)
{
  int ix;
  int jx;
  int status;
  json_t * postdata;
  json_t * params;
  json_t * param;
  json_t * part;
  struct curl_slist * headers = NULL;
  struct curl_httppost * result = NULL;
  struct curl_httppost * last = NULL;
  struct curl_forms options[8];
  
  //fprintf(stderr, "har_request_postdata_to_curl_httppost\n");
  if (!req || !json_is_object(req)) return NULL;
  postdata = json_object_get(req, "postData");
  if (!postdata || !json_is_object(postdata)) return NULL;
  part = json_object_get(postdata, "mimeType");
  if (part && json_is_string(part)) {
    fprintf(stderr, "request.postData.mimeType\n");
    json_object_set_new(req, "_contentType", part);
  }

  //fprintf(stderr, "har_request_postdata_to_curl_httppost (postdata)\n");
  params = json_object_get(postdata, "params");
  if (!params || !json_is_array(params)) return NULL;
  //fprintf(stderr, "har_request_postdata_to_curl_httppost (params)\n");
  
  json_array_foreach(params, ix, param) {
    jx = 0;
    
    if (!json_is_object(param)) continue;

    part = json_object_get(param, "name");
    if (part && json_is_string(part)) {
      //fprintf(stderr, "har_request_postdata_to_curl_httppost (name)\n");
      options[jx].option = CURLFORM_COPYNAME;
      options[jx].value = json_string_value(part);
      jx++;
    }

    part = json_object_get(param, "value");
    if (part && json_is_string(part)) {
      //fprintf(stderr, "har_request_postdata_to_curl_httppost (value)\n");
      options[jx].option = CURLFORM_COPYCONTENTS;
      options[jx].value = json_string_value(part);
      jx++;
    }

    part = json_object_get(param, "file");
    if (part && json_is_string(part)) {
      options[jx].option = CURLFORM_FILE;
      options[jx].value = json_string_value(part);
      jx++;
    }
    
    part = json_object_get(param, "fileName");
    if (part && json_is_string(part)) {
      options[jx].option = CURLFORM_FILENAME;
      options[jx].value = json_string_value(part);
      jx++;
    }
    
    part = json_object_get(param, "contentType");
    if (part && json_is_string(part)) {
      options[jx].option = CURLFORM_CONTENTTYPE;
      options[jx].value = json_string_value(part);
      jx++;
    }
    
    part = json_object_get(param, "headers");
    if (part && json_is_array(part)) {
      headers = har_headers_to_curl_slist(part);
      options[jx].option = CURLFORM_CONTENTHEADER;
      options[jx].value = (const char *)headers;
      jx++;
    }

    options[jx].option = CURLFORM_END;
    status = curl_formadd(&result, &last,
                          CURLFORM_ARRAY, (struct curl_forms *)&options,
                          CURLFORM_END);
    
    if (status) {
      char buf[1024];
      har_curl_formadd_strerror(status, buf, sizeof(buf));
      fprintf(stderr, "curl_formadd gave us %d %s\n", status, buf);
    }
  }

  return result;
}

int
har_request_postdata_to_byte_array(json_t * req, GByteArray * bytes)
{
  json_t * postdata;
  json_t * params;
  json_t * enc_part;
  json_t * text_part;
  json_t * type_part;

  size_t size = 0;
  const char * encoding = NULL;
  const char * text = NULL;
  const char * type = NULL;
  
  fprintf(stderr, "har_request_postdata_to_byte_array\n");
  if (!req || !json_is_object(req)) return 0;
  postdata = json_object_get(req, "postData");
  if (!postdata || !json_is_object(postdata)) return 0;

  params = json_object_get(postdata, "params");
  enc_part = json_object_get(postdata, "encoding");
  text_part = json_object_get(postdata, "text");
  type_part = json_object_get(req, "_contentType");
  
  if (params && text_part) {
    return HAR_ERROR_TEXT_AND_PARAMS;
  } else if (params && json_is_object(params)) {
    return 0;
  } else if (text_part && json_is_string(text_part)) {
    type = json_string_value(type_part);
    text = json_string_value(text_part);
    if (enc_part && json_is_string(enc_part) &&
        g_ascii_strcasecmp(json_string_value(enc_part), "base64")) {
      fprintf(stderr, "request.postData.text (base64)\n");
      text = (const char *)g_base64_decode(text, &size);
      g_byte_array_append(bytes, (const guint8 *)text, size);
    } else {
      fprintf(stderr, "request.postData.text (plain)\n");
      size = strlen(json_string_value(text_part));
      text = json_string_value(text_part);
      g_byte_array_append(bytes, (const guint8 *)text, size);
    }
  }

  return 0;
}

void
har_response_headers_from_byte_array(json_t * resp, GByteArray * bytes)
{
  //fprintf(stderr, "har_response_headers_from_byte_array\n");
  int ix;
  guint s_len = bytes->len;
  const char * s = (const char *)(bytes->data);
  json_object_set_new(resp, "headersSize", json_integer(s_len));
  if (global_verbose) {
    json_object_set_new(resp, "headersText", json_string(s));
  }
  
  json_t * header;
  json_t * headers = json_object_get(resp, "headers");
  if (!headers || !json_is_array(headers)) {
    json_object_set_new(resp, "headers", json_array());
    headers = json_object_get(resp, "headers");
  }

  har_headers_from_text(headers, s, s_len);

  json_array_foreach(headers, ix, header) {
    const char * name = json_string_value(json_object_get(header, "name"));
    
    if (!g_ascii_strcasecmp(name, "content-encoding")) {
      const char * value = json_string_value(json_object_get(header, "value"));
      json_object_set_new(resp, "_contentEncoding", json_string(value));
    }

    if (!g_ascii_strcasecmp(name, "content-type")) {
      const char * value = json_string_value(json_object_get(header, "value"));
      json_object_set_new(resp, "_contentType", json_string(value));
    }
  }
  
  return;
}

int
har_window_bits(const char * content_encoding)
{
  /*
   * This is the super secret code for zlib
   *
   * windowBits = -MAX_WBITS      // means use deflate w/o zlib
   * windowBits = MAX_WBITS | 16  // means use gzip
   * windowBits = MAX_WBITS       // means use deflate with zlib wrapper
   *
   * and for some reason it is documented nowhere,
   * and yet understood by everyone...
   *
   * Welcome to open source!
   */

  if (content_encoding == NULL) {
    return -1; // error
  } else if (!g_ascii_strcasecmp(content_encoding, "gzip")) {
    return (MAX_WBITS | 16);
  } else if (!g_ascii_strcasecmp(content_encoding, "deflate")) { /* wrapped in zlib */
    return (MAX_WBITS);
  } else if (!g_ascii_strcasecmp(content_encoding, "deflate-w-o-zlib")) {
    return (-MAX_WBITS);
  }
  //} else if (!g_ascii_strcasecmp(content_encoding, "bzip2")) {
  //  return -1; // not supported
  //} else if (!g_ascii_strcasecmp(content_encoding, "sdch")) {
  //  return -1; // not supported
  //} else if (!g_ascii_strcasecmp(content_encoding, "lzma")) {
  //  return -1; // not supported
  //} else if (!g_ascii_strcasecmp(content_encoding, "xz")) {
  //  return -1; // not supported
  //}
  
  return 0;
}

int
har_uncompress(gpointer dest_data, gsize * dest_len,
               gconstpointer src_data, gsize src_len,
               int windowBits)
{

  int ret;
  z_stream stream;
  memset(&stream, 0, sizeof(stream));

  stream.next_in = (Bytef *)src_data;
  stream.avail_in = src_len;
  stream.next_out = (Bytef *)dest_data;
  stream.avail_out = *dest_len;
  if (src_len == 0) {
    return Z_DATA_ERROR;
  }
  
  ret = inflateInit2(&stream, windowBits);
  if (ret != Z_OK) {
    return ret;
  }

  ret = inflate(&stream, Z_NO_FLUSH);
  (void)inflateEnd(&stream);
  if (ret != Z_STREAM_END) {
    return ret;
  }

  *dest_len = stream.total_out;
  fprintf(stderr, "avail_out: %d (should be zero)\n", stream.avail_out);
  
  return ret == Z_STREAM_END ? Z_OK : ret;
}

GBytes *
har_bytes_uncompress(const GBytes * src, int windowBits)
{
  gsize src_len;
  gconstpointer src_data = g_bytes_get_data((GBytes *)src, &src_len);
  fprintf(stderr, "windowBits = %d\n", windowBits);
  fprintf(stderr, "src_len = %lu\n", src_len);
  gsize dest_len = ((size_t)(((float)(src_len)) * 2.0f)) + 24;
  fprintf(stderr, "dest_len = %lu\n", dest_len);
  gpointer dest_data = g_malloc(dest_len);
  int status;
  
  if ((status = har_uncompress(dest_data, &dest_len, src_data, src_len, windowBits)) != Z_OK) {
    char buf[1024];
    har_zlib_strerror(status, buf, sizeof(buf));
    fprintf(stderr, "there was an error with zlib: %d %s\n", status, buf);
    return (GBytes *)src;
  }
  
  return g_bytes_new(dest_data, dest_len);
}

GByteArray *
har_byte_array_uncompress(GByteArray * src, int windowBits)
{
  return g_bytes_unref_to_array(har_bytes_uncompress(g_byte_array_free_to_bytes(src), windowBits));
}

//const char *
//har_uncompress_gzip(const char * data, size_t size, size_t * result_size)
//{
//  const char * result = g_malloc0(*result_size);
//  uncompress((Bytef *)result, result_size, (const Bytef *)data, size);
//  return result;
//}

void
har_response_content_from_byte_array(json_t * resp, GByteArray * bytes)
{
  //fprintf(stderr, "har_response_content_from_byte_array\n");
  guint size = bytes->len;
  const char * text = (const char *)(bytes->data);
  const char * end = NULL;
  json_t * part;
  json_t * content = json_object_get(resp, "content");
  const char * encoding = NULL;

  if (g_utf8_validate(text, size, &end)) {
    json_object_set_new(content, "text", json_string(text));
  } else {
    text = g_base64_encode((const guchar *)text, size);
    json_object_set_new(content, "text", json_string(text));
    json_object_set_new(content, "encoding", json_string("base64"));
  }

  return;
}

int
har_debug_callback(CURL * easy,
                   curl_infotype type,
                   char * data,
                   size_t size,
                   void * entryptr)
{
  int request_header_index = 0;
  int response_header_index = 0;
  json_t * req;
  json_t * resp;
  json_t * part;
  json_t * entry = (json_t *)entryptr;
  const char * debug_key = "_debugCurlInfo";
  //fprintf(stderr, "har_debug_callback %d\n", type);

  switch (type) {
    
  case CURLINFO_TEXT:
    //fprintf(stderr, "har_debug_callback text = %s\n", data);
    if (global_verbose) {
      part = json_object_get(entry, debug_key);
      if (!part || !json_is_array(part)) {
        json_object_set_new(entry, debug_key, json_array());
        part = json_object_get(entry, debug_key);
      }
      json_array_append_new(part, json_string(g_strdup(data)));
    }
    break;
    
  case CURLINFO_HEADER_OUT:
    //fprintf(stderr, "har_debug_callback header_out = %s of %d\n", data, request_header_index);
    req = json_object_get(entry, "request");
    assert(req && json_is_object(req));

    /* save headersSize */
    char * s = g_malloc0(size);
    strncpy(s, data, size);
    json_object_set_new(req, "headers", json_array());
    json_t * headers = json_object_get(req, "headers");

    json_object_set_new(req, "headersSize", json_integer(size));
    if (global_verbose) {
      har_headers_from_text(headers, s, size);
      
      json_object_set_new(req, "headersText", json_string(g_strdup(s)));

      /* save requestLine */
      const char * end = g_strstr_len(data, size, "\r\n");
      strncpy(s, data, (end - data));
      s[(end - data)] = '\0';
      json_object_set_new(req, "requestLine", json_string(g_strdup(s)));
    }
    request_header_index += 1;
    break;
    
  case CURLINFO_HEADER_IN:
    //fprintf(stderr, "har_debug_callback header_in = %s of %d\n", data, response_header_index);
    resp = json_object_get(entry, "response");
    if (size >= 5 && data[4] == '/' && response_header_index == 0 && global_verbose) {
      char * s = g_malloc0(size);
      const char * end = g_strstr_len(data, size, "\r\n");
      strncpy(s, data, (end - data));
      s[(end - data)] = '\0';
      json_object_set_new(resp, "statusLine", json_string(g_strdup(s)));
    }
    response_header_index += 1;
    break;
    
  case CURLINFO_DATA_OUT:
    //fprintf(stderr, "har_debug_callback data_out # %lu\n", (uintptr_t)size);
    req = json_object_get(entry, "request");
    if (!req || !json_is_object(req)) return 0;
    part = json_object_get(req, "postData");
    if (!part || !json_is_object(part)) return 0;

    json_object_set_new(part, "text", json_string(g_strdup(data)));
    json_object_set_new(req, "bodySize", json_integer(size));
    if (global_verbose) json_object_set_new(part, "size", json_integer(size));
    break;
    
  case CURLINFO_DATA_IN:
    //fprintf(stderr, "har_debug_callback data_in # %lu\n", (uintptr_t)size);
    resp = json_object_get(entry, "response");
    if (!resp || !json_is_object(resp)) return 0;
    part = json_object_get(resp, "content");
    if (!part || !json_is_object(part)) return 0;

    json_object_set_new(resp, "bodySize", json_integer(size));
    json_object_set_new(part, "size", json_integer(size));
    break;
    
  //case CURLINFO_SSL_DATA_OUT:
  //  fprintf(stderr, "har_debug_callback ssl_data_out # %lu\n", (uintptr_t)size);
  //  break;
  //case CURLINFO_SSL_DATA_IN:
  //  fprintf(stderr, "har_debug_callback ssl_data_in # %lu\n", (uintptr_t)size);
  //  break;
  //case CURLINFO_END:
  //  fprintf(stderr, "har_debug_callback end\n");
  //  break;
  default:
    break;
  }
  
  return 0;
}

size_t
har_read_callback(void * ptr,
                  size_t size,
                  size_t nitems,
                  void * bytesptr)
{
  size_t ptrlen = size*nitems;
  GByteArray * bytes = (GByteArray *)bytesptr;
  //fprintf(stderr, "har_read_callback %lu\n", (uintptr_t)ptrlen);
  if (ptrlen > bytes->len)
    ptrlen = bytes->len;
  memcpy(ptr, bytes->data, ptrlen);
  if (ptrlen > 0)
    bytes = g_byte_array_remove_range(bytes, 0, ptrlen);

  return ptrlen;
}

size_t
har_write_callback(const void * ptr,
                   size_t size,
                   size_t nitems,
                   void * bytesptr)
{
  size_t ptrlen = size*nitems;
  GByteArray * bytes = (GByteArray *)bytesptr;
  //fprintf(stderr, "har_write_callback %lu\n", (uintptr_t)ptrlen);
  bytes = g_byte_array_append(bytes, ptr, ptrlen);
  return ptrlen;
}

size_t
har_header_callback(const void * ptr,
                   size_t size,
                   size_t nitems,
                   void * bytesptr)
{
  size_t ptrlen = size*nitems;
  GByteArray * bytes = (GByteArray *)bytesptr;
  //fprintf(stderr, "har_header_callback %lu\n", (uintptr_t)ptrlen);
  bytes = g_byte_array_append(bytes, ptr, ptrlen);
  return ptrlen;
}

curl_socket_t
har_socket_open_callback(void * clientp,
                         curlsocktype purpose,
                         struct curl_sockaddr * address)
{
  return (curl_socket_t)0;
}

int
har_socket_close_callback(void * clientp, curl_socket_t item)
{
  return 0;
}

int
har_sockopt_callback(void * clientp,
                     curl_socket_t curlfd,
                     curlsocktype purpose)
{
  return 0;
}

long chunk_begin_callback(const void * transfer_info,
                                 void * ptr,
                                 int remains)
{
  return 0;
}

long chunk_end_callback(void * ptr)
{
  return 0;
}

int
har_method_to_curl_method(const char * method, bool auto_method, CURL * easy)
{
  /* libcurl has a very complicated way to set 'method' */
  if (!g_ascii_strcasecmp(method, "GET")) {
    gint curl_method = CURLOPT_HTTPGET;
    curl_easy_setopt(easy, curl_method, TRUE);
  } else if (!g_ascii_strcasecmp(method, "POST")) {
    gint curl_method = CURLOPT_POST;
    curl_easy_setopt(easy, curl_method, TRUE);
  } else if (!g_ascii_strcasecmp(method, "PUT")) {
    gint curl_method = CURLOPT_PUT;
    curl_easy_setopt(easy, curl_method, TRUE);
  } else if (!g_ascii_strcasecmp(method, "HEAD")) {
    gint curl_method = CURLOPT_NOBODY;
    curl_easy_setopt(easy, curl_method, TRUE);
  } else {
    
    /* Other methods to consider adding:
     * CURLOPT_COPYPOSTFIELDS
     * CURLOPT_POSTFIELDS
     */

    /* Make sure that we pass the method in uppercase. */
    const char * curl_method = g_ascii_strup(method, strnlen(method, 16));
    curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, curl_method);

    /* "Before [libcurl] version 7.17.0, strings were not copied" 
     * but they are now. This provides a minimum version number
     * for which libcurl we are compatible with.
     */
    g_free((gpointer)curl_method);
  }

  return HAR_OK;
}

int
har_request_to_curl_url(json_t * req,
                        CURL * easy)
{
  json_t * part = json_object_get(req, "url");
  json_t * pj;
  const char * raw_url = json_string_value(part);
  const char * url = raw_url;
  const char * ks;
  const char * vs;
  int ix;
  part = json_object_get(req, "queryString");

  if (part && json_is_array(part) && json_array_size(part)) {
    json_t * query = part;
    GByteArray * bytes = g_byte_array_new();
    bytes = g_byte_array_append(bytes, (const guint8 *)raw_url, strlen(raw_url));

    json_array_foreach(query, ix, pj) {
      ks = json_string_value(json_object_get(pj, "name"));
      vs = json_string_value(json_object_get(pj, "value"));
      ks = curl_easy_escape(easy, ks, strlen(ks));
      vs = curl_easy_escape(easy, vs, strlen(vs));
      size_t s_len = strlen(ks) + strlen(vs) + 5;
      char * s = g_malloc0(s_len);
      snprintf(s, s_len, "&%s=%s", ks, vs);
      bytes = g_byte_array_append(bytes, (const guint8 *)s, s_len);
    }
    bytes = g_byte_array_append(bytes, (const guint8 *)"\0", 1);
    url = (const char *)bytes->data;
    g_byte_array_free(bytes, false);
  }
  
  /* TODO: handle separate fields */
  curl_easy_setopt(easy, CURLOPT_URL, url);

  return HAR_OK;
}

int
har_entry_to_curl_easy_setopt(json_t * obj, CURL * easy,
                              GByteArray * harbodyin,
                              GByteArray * harheadout,
                              GByteArray * harbodyout)
{
  int status;
  json_t * entry = obj;
  json_t * req = json_object_get(entry, "request");
  json_t * resp = json_object_get(entry, "response");
  json_t * part;
  struct curl_httppost * formpost;
  
  json_incref(entry);
  
  if (!req) {
    return HAR_ERROR_NO_REQUEST;
  }

  if (!resp) {
    return HAR_ERROR_NO_RESPONSE;
  }
  
  if ((part = json_object_get(req, "method")) && json_is_string(part)) {
    const char * method = json_string_value(part);
    har_method_to_curl_method(method, false, easy);
  } else {
    return HAR_ERROR_NO_METHOD;
  }

  if ((part = json_object_get(req, "url")) && json_is_string(part)) {
    har_request_to_curl_url(req, easy);
  } else {
    return HAR_ERROR_NO_URL;
  }

  /* install debug callback */
  curl_easy_setopt(easy, CURLOPT_DEBUGDATA, entry);
  curl_easy_setopt(easy, CURLOPT_DEBUGFUNCTION, &har_debug_callback);
  curl_easy_setopt(easy, CURLOPT_VERBOSE, 1);
  
  /* install header list for request headers */
  struct curl_slist * headers = har_request_to_curl_slist(req);
  if (headers) {
    curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
    //curl_easy_setopt(easy, CURLOPT_HEADEROPT, CURLHEADER_UNIFIED);
  }
  
  /* install read callback for request body */
  part = json_object_get(req, "postData");
  if (!part || !json_is_object(part)) {
    json_object_set_new(req, "postData", json_object());
    part = json_object_get(req, "postData");
  }
  status = har_request_postdata_to_byte_array(req, harbodyin);
  formpost = har_request_postdata_to_curl_httppost(req);
  
  if (status) {
    fprintf(stderr, "both params and text\n");
    return status;
  } else if (formpost) {
    fprintf(stderr, "request.postData.params\n");
    curl_easy_setopt(easy, CURLOPT_HTTPPOST, formpost);
  } else if (harbodyin->len) {
    fprintf(stderr, "request.postData.text\n");
    //curl_easy_setopt(easy, CURLOPT_READDATA, harbodyin);
    //curl_easy_setopt(easy, CURLOPT_READFUNCTION, &har_read_callback);
    curl_easy_setopt(easy, CURLOPT_POSTFIELDS, harbodyin->data);
    curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, harbodyin->len);
  }
  
  /* install header callback for response headers */
  json_object_set_new(resp, "headers", json_array());
  curl_easy_setopt(easy, CURLOPT_HEADERDATA, harheadout);
  curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, &har_header_callback);

  /* install write callback for response body */
  json_object_set_new(resp, "content", json_object());
  curl_easy_setopt(easy, CURLOPT_WRITEDATA, harbodyout);
  curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, &har_write_callback);

  //json_decref(req);
  //json_decref(entry);
  return HAR_OK;
}

int
har_entry_from_curl_easy_getinfo(json_t * obj, CURL * easy,
                                 GByteArray * harheadout,
                                 GByteArray * harbodyout)
{
  json_t * entry = obj;
  json_t * req = json_object_get(entry, "request");
  json_t * resp = json_object_get(entry, "response");
  json_t * part;

  long status;
  curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status);
  json_object_set_new(resp, "status", json_integer(status));

  const char * redirect_url;
  curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &redirect_url);
  if (redirect_url) {
    json_object_set_new(resp, "redirectURL", json_string(g_strdup(redirect_url)));
  }

  /* finish up with write callback */
  har_response_headers_from_byte_array(resp, harheadout);

  // TODO: GET content-encoding header
  //const char * content_encoding = "identity";
  part = json_object_get(resp, "_contentEncoding");
  if (part) {
    const char * content_encoding = json_string_value(part);
    fprintf(stderr, "content_encoding = %s\n", content_encoding);
    int windowBits = har_window_bits(content_encoding);
    if (windowBits == -1) {
      fprintf(stderr, "unrecognized Content-Encoding\n");
    } else if (windowBits != 0) {
      harbodyout = har_byte_array_uncompress(harbodyout, windowBits);
    }
  }
  
  har_response_content_from_byte_array(resp, harbodyout);

  return HAR_OK;
}

int
main(int argc, char *argv[])
{
  CURLcode ret;
  CURL * easy;
  int status;
  size_t flags;
  json_t * entry;
  json_t * resp;
  json_t * req;
  json_t * part;
  json_error_t parse_error;
  GError * option_error;
  GOptionContext * options;

  /* temp vars */
  GByteArray * harbodyin = g_byte_array_new();
  GByteArray * harheadout = g_byte_array_new();
  GByteArray * harbodyout = g_byte_array_new();
  GOptionEntry option_entries[] = {
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &global_verbose,
      "A switch that is in the test group", NULL },
    NULL
  };
  
  /* parse args */
  options = g_option_context_new("harcurl (" PACKAGE_VERSION ")");
  g_option_context_add_main_entries(options, option_entries, NULL);
  if (g_option_context_parse(options, &argc, &argv, &option_error) != TRUE) {
    fprintf(stderr, "error parsing options\n");
  }
  
  /* load json */
  flags = 0;
  entry = json_loadf(stdin, flags, &parse_error);
  if (!entry) {
    fprintf(stderr, "no JSON could be decoded on standard input\n");
    return HAR_ERROR_WITH_JSON;
  }
  json_object_set_new(entry, "response", json_object());
  resp = json_object_get(entry, "response");
  json_object_set_new(resp, "headersSize", json_integer(0));
  json_object_set_new(resp, "bodySize", json_integer(0));

  req = json_object_get(entry, "request");
  if (!req || !json_is_object(req)) {
    return HAR_ERROR_NO_REQUEST;
  }
  part = json_object_get(req, "postData");
  if (!part || !json_is_object(part)) {
    json_object_set_new(req, "postData", json_object());
    part = json_object_get(req, "postData");
    json_object_set_new(req, "headersSize", json_integer(0));
    json_object_set_new(req, "bodySize", json_integer(0));
    if (global_verbose)
      json_object_set_new(part, "size", json_integer(0));
  }

  /* init curl */
  easy = curl_easy_init();
  if (!easy) {
    fprintf(stderr, "no curl_easy handle\n");
    return HAR_ERROR_WITH_CURL;
  }

  /* transform */
  status = har_entry_to_curl_easy_setopt(entry, easy, harbodyin, harheadout, harbodyout);
  if (status != HAR_OK) {
    fprintf(stderr, "unable to transform har_entry object to curl_easy handle\n");
    return status;
  }

  /* perform */
  //fprintf(stderr, "perform\n");
  ret = curl_easy_perform(easy);
  if (ret != CURLE_OK) {
    fprintf(stderr, "something happend during perform of the curl_easy handle\n");
  }
  
  /* transform */
  status = har_entry_from_curl_easy_getinfo(entry, easy, harheadout, harbodyout);
  if (status != HAR_OK) {
    fprintf(stderr, "unable to transform curl_easy handle to har_entry object\n");
    return status;
  }

  /* free curl */
  curl_easy_cleanup(easy);
  easy = NULL;

  /* dump json */
  flags = JSON_SORT_KEYS | JSON_INDENT(2);
  status = json_dumpf(entry, stdout, flags);
  if (status) {
    fprintf(stderr, "something happend during dump of the har_entry object\n");
    return status;
  }

  return (int)ret;
}
